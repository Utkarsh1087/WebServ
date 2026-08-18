#include "Server.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>

// Constructor initializes members
Server::Server(int port) 
    : m_port(port), m_listenSocket(INVALID_SOCKET), m_isRunning(false) {}

// Destructor ensures the server is stopped and sockets are cleaned up
Server::~Server() {
    stop();
}

bool Server::init() {
    std::cout << "[INFO] Initializing WebServ on port " << m_port << "...\n";

    // 1. Initialize Winsock DLL.
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "[ERROR] WSAStartup failed with error: " << result << "\n";
        return false;
    }

    // 2. Create the listening TCP socket.
    m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenSocket == INVALID_SOCKET) {
        std::cerr << "[ERROR] socket creation failed with error: " << WSAGetLastError() << "\n";
        WSACleanup();
        return false;
    }

    // 3. Bind the socket.
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY; 
    serverAddr.sin_port = htons(m_port);     

    result = bind(m_listenSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (result == SOCKET_ERROR) {
        std::cerr << "[ERROR] bind failed with error: " << WSAGetLastError() << "\n";
        closesocket(m_listenSocket);
        WSACleanup();
        return false;
    }

    // 4. Start listening for incoming connections.
    result = listen(m_listenSocket, SOMAXCONN);
    if (result == SOCKET_ERROR) {
        std::cerr << "[ERROR] listen failed with error: " << WSAGetLastError() << "\n";
        closesocket(m_listenSocket);
        WSACleanup();
        return false;
    }

    std::cout << "[SUCCESS] Socket bound and listening on port " << m_port << "\n";
    return true;
}

void Server::start() {
    m_isRunning = true;
    std::cout << "[INFO] Server started. Waiting for connections...\n";

    while (m_isRunning) {
        SOCKET clientSocket = accept(m_listenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            if (m_isRunning) {
                std::cerr << "[WARNING] accept failed with error: " << WSAGetLastError() << "\n";
            }
            continue;
        }

        // Process client request
        handleClient(clientSocket);
    }
}

void Server::stop() {
    if (m_isRunning) {
        m_isRunning = false;
        std::cout << "[INFO] Stopping server...\n";
    }

    if (m_listenSocket != INVALID_SOCKET) {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
    }

    WSACleanup();
    std::cout << "[INFO] Server stopped clean.\n";
}

void Server::addRoute(const std::string& method, const std::string& path, RouteHandler handler) {
    m_router.addRoute(method, path, handler);
}

void Server::handleClient(SOCKET clientSocket) {
    const int bufferSize = 4096;
    std::vector<char> buffer(bufferSize, 0);
    
    // 1. Read the initial chunk of the HTTP request from the socket
    int bytesReceived = recv(clientSocket, buffer.data(), bufferSize - 1, 0);
    if (bytesReceived <= 0) {
        closesocket(clientSocket);
        return;
    }

    buffer[bytesReceived] = '\0';
    std::string rawRequest(buffer.data(), bytesReceived);

    // 2. Parse headers and initial body
    HttpRequest request = HttpParser::parse(rawRequest);
    if (!request.isParsedSuccessfully) {
        Router::sendResponse(clientSocket, 400, "Bad Request", "text/plain", "400 Bad Request: Malformed HTTP request");
        closesocket(clientSocket);
        return;
    }

    // 3. Handle Request Body (Content-Length verification for POST/PUT)
    auto lenIt = request.headers.find("content-length");
    if (lenIt != request.headers.end()) {
        try {
            size_t contentLength = std::stoull(lenIt->second);
            
            // Check if we have received the full body payload yet.
            // If the body we have currently is smaller than Content-Length, we must read more bytes.
            while (request.body.length() < contentLength) {
                std::vector<char> bodyBuf(bufferSize, 0);
                int bodyBytes = recv(clientSocket, bodyBuf.data(), bufferSize - 1, 0);
                if (bodyBytes <= 0) {
                    break; // Error or socket closed early
                }
                request.body.append(bodyBuf.data(), bodyBytes);
            }
        } catch (...) {
            Router::sendResponse(clientSocket, 400, "Bad Request", "text/plain", "400 Bad Request: Invalid Content-Length");
            closesocket(clientSocket);
            return;
        }
    }

    // Print request information
    std::cout << "\n----------------------------------------\n";
    std::cout << "[REQUEST] " << request.method << " " << request.uri << " (" << request.body.length() << " body bytes)\n";
    std::cout << "----------------------------------------\n";

    // 4. Security Check: Path Traversal Protection
    if (request.uri.find("..") != std::string::npos) {
        Router::sendResponse(clientSocket, 400, "Bad Request", "text/plain", "400 Bad Request: Invalid Path");
        closesocket(clientSocket);
        return;
    }

    // 5. Try Routing (Dynamic API endpoints take precedence)
    if (m_router.route(request, clientSocket)) {
        closesocket(clientSocket);
        return;
    }

    // 6. Fallback: Static File Serving (Only allows GET and HEAD requests)
    if (request.method != "GET" && request.method != "HEAD") {
        Router::sendResponse(clientSocket, 405, "Method Not Allowed", "text/plain", "405 Method Not Allowed");
        closesocket(clientSocket);
        return;
    }

    // Resolve URL to local file path
    std::string uri = request.uri;
    if (uri == "/" || uri.empty()) {
        uri = "/index.html";
    }
    std::string localFilePath = "./public" + uri;

    bool fileFound = false;
    std::string fileContent = readFile(localFilePath, fileFound);

    std::string statusLine;
    std::string mimeType;
    std::string responseBody;

    if (fileFound) {
        statusLine = "HTTP/1.1 200 OK\r\n";
        mimeType = getMimeType(localFilePath);
        responseBody = fileContent;
    } else {
        bool custom404Found = false;
        std::string custom404Content = readFile("./public/404.html", custom404Found);
        
        statusLine = "HTTP/1.1 404 Not Found\r\n";
        mimeType = "text/html; charset=utf-8";
        
        if (custom404Found) {
            responseBody = custom404Content;
        } else {
            responseBody = "<html><body><h1>404 Not Found</h1><p>The requested URL was not found on this server.</p></body></html>";
        }
    }

    // 7. Write Response Headers
    std::ostringstream responseStream;
    responseStream << statusLine
                   << "Content-Type: " << mimeType << "\r\n"
                   << "Content-Length: " << responseBody.length() << "\r\n"
                   << "Connection: close\r\n"
                   << "\r\n"; // Blank line

    std::string headers = responseStream.str();
    send(clientSocket, headers.c_str(), (int)headers.length(), 0);

    // 8. Write Response Body (Omitted if the client requested HEAD)
    if (request.method != "HEAD") {
        send(clientSocket, responseBody.c_str(), (int)responseBody.length(), 0);
    }

    std::cout << "[RESPONSE SENT] " << (fileFound ? "200 OK" : "404 Not Found") << "\n";
    closesocket(clientSocket);
}

std::string Server::readFile(const std::string& filePath, bool& found) {
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        found = false;
        return "";
    }

    found = true;
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (file.read(buffer.data(), size)) {
        return std::string(buffer.data(), size);
    }
    
    return "";
}

std::string Server::getMimeType(const std::string& filePath) {
    size_t dotPos = filePath.find_last_of('.');
    if (dotPos == std::string::npos) {
        return "application/octet-stream";
    }

    std::string ext = filePath.substr(dotPos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
    if (ext == ".css") return "text/css";
    if (ext == ".js") return "application/javascript";
    if (ext == ".json") return "application/json";
    if (ext == ".png") return "image/png";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".gif") return "image/gif";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".ico") return "image/x-icon";
    
    return "application/octet-stream";
}
