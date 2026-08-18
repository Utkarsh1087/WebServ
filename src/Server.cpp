#include "Server.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

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
    // MAKEWORD(2, 2) requests version 2.2 of Winsock.
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) {
        std::cerr << "[ERROR] WSAStartup failed with error: " << result << "\n";
        return false;
    }

    // 2. Create the listening TCP socket.
    // AF_INET specifies the IPv4 address family.
    // SOCK_STREAM specifies a stream socket, which implies TCP.
    // IPPROTO_TCP specifies the TCP protocol.
    m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenSocket == INVALID_SOCKET) {
        std::cerr << "[ERROR] socket creation failed with error: " << WSAGetLastError() << "\n";
        WSACleanup();
        return false;
    }

    // 3. Bind the socket.
    // We bind the socket to the port and specify that we accept connections from any IP interface (INADDR_ANY).
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY; // Listen on all network interfaces (Ethernet, Wi-Fi, localhost)
    serverAddr.sin_port = htons(m_port);     // htons converts host byte order to network byte order (Big Endian)

    result = bind(m_listenSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (result == SOCKET_ERROR) {
        std::cerr << "[ERROR] bind failed with error: " << WSAGetLastError() << "\n";
        closesocket(m_listenSocket);
        WSACleanup();
        return false;
    }

    // 4. Start listening for incoming connections.
    // SOMAXCONN is a constant representing the maximum backlog queue of pending connections.
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
        // accept() blocks the execution thread until a client attempts to connect to our server.
        // It returns a new client socket that we use to communicate with this specific client.
        SOCKET clientSocket = accept(m_listenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) {
            // If the server is stopped, accept() will fail. We check if we are still running.
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

    // Cleans up Winsock DLL resources
    WSACleanup();
    std::cout << "[INFO] Server stopped clean.\n";
}

void Server::handleClient(SOCKET clientSocket) {
    // 1. Read request from client
    const int bufferSize = 4096;
    std::vector<char> buffer(bufferSize, 0);
    
    // recv() reads data from the socket stream into our buffer.
    // In a blocking setup, it blocks until data is available.
    int bytesReceived = recv(clientSocket, buffer.data(), bufferSize - 1, 0);
    if (bytesReceived <= 0) {
        // Connection closed or failed
        closesocket(clientSocket);
        return;
    }

    // Ensure null-termination to treat the buffer as a string safely
    buffer[bytesReceived] = '\0';
    std::string rawRequest(buffer.data());

    // Print incoming request to the console for tracking
    std::cout << "\n----------------------------------------\n";
    std::cout << "[REQUEST RECEIVED] Bytes: " << bytesReceived << "\n";
    
    // Print just the first few lines of the request so we don't flood the console
    std::string firstLines = rawRequest.substr(0, rawRequest.find("\r\n\r\n"));
    std::cout << firstLines << "\n";
    std::cout << "----------------------------------------\n";

    // 2. Parse the Request Line (First line of HTTP request)
    // Format: METHOD URI HTTP-VERSION (e.g. GET /index.html HTTP/1.1)
    std::istringstream requestStream(rawRequest);
    std::string method, uri, httpVersion;
    requestStream >> method >> uri >> httpVersion;

    // Validate request structure (HTTP/1.1 requires these three fields)
    if (method.empty() || uri.empty() || httpVersion.empty()) {
        std::string badRequestResponse = 
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 15\r\n"
            "Connection: close\r\n\r\n"
            "400 Bad Request";
        send(clientSocket, badRequestResponse.c_str(), (int)badRequestResponse.length(), 0);
        closesocket(clientSocket);
        return;
    }

    // 3. Resolve file path
    // For safety and routing, default `/` to `/index.html`
    if (uri == "/" || uri.empty()) {
        uri = "/index.html";
    }

    // In a production server, we MUST protect against path traversal (e.g., GET /../../Windows/system.ini)
    // We will do a simple check here: if the URI contains "..", reject it with 400.
    if (uri.find("..") != std::string::npos) {
        std::string forbiddenResponse = 
            "HTTP/1.1 400 Bad Request\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 29\r\n"
            "Connection: close\r\n\r\n"
            "400 Bad Request: Invalid Path";
        send(clientSocket, forbiddenResponse.c_str(), (int)forbiddenResponse.length(), 0);
        closesocket(clientSocket);
        return;
    }

    // Target path in our public directory
    std::string localFilePath = "./public" + uri;

    // 4. Serve the requested file
    bool fileFound = false;
    std::string fileContent = readFile(localFilePath, fileFound);

    std::string statusLine;
    std::string mimeType;
    std::string body;

    if (fileFound) {
        statusLine = "HTTP/1.1 200 OK\r\n";
        mimeType = getMimeType(localFilePath);
        body = fileContent;
    } else {
        // Serve custom 404 page if it exists
        bool custom404Found = false;
        std::string custom404Content = readFile("./public/404.html", custom404Found);
        
        statusLine = "HTTP/1.1 404 Not Found\r\n";
        mimeType = "text/html";
        
        if (custom404Found) {
            body = custom404Content;
        } else {
            body = "<html><body><h1>404 Not Found</h1><p>The requested URL was not found on this server.</p></body></html>";
        }
    }

    // 5. Send HTTP Response
    std::ostringstream responseStream;
    responseStream << statusLine
                   << "Content-Type: " << mimeType << "\r\n"
                   << "Content-Length: " << body.length() << "\r\n"
                   << "Connection: close\r\n"
                   << "\r\n" // Crucial blank line separating headers from body
                   << body;

    std::string fullResponse = responseStream.str();
    
    // send() writes bytes back into the client TCP socket stream
    int bytesSent = send(clientSocket, fullResponse.c_str(), (int)fullResponse.length(), 0);
    if (bytesSent == SOCKET_ERROR) {
        std::cerr << "[WARNING] send failed with error: " << WSAGetLastError() << "\n";
    } else {
        std::cout << "[RESPONSE SENT] Status: " << (fileFound ? "200 OK" : "404 Not Found") 
                  << " | Sent: " << bytesSent << " bytes\n";
    }

    // 6. Close connection (HTTP/1.1 Connection: close behavior)
    closesocket(clientSocket);
}

std::string Server::readFile(const std::string& filePath, bool& found) {
    // Open in binary mode so we preserve exact byte structure of images, pdfs, etc.
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        found = false;
        return "";
    }

    found = true;
    
    // Get size of file by checking the position of the pointer (which is at the end because of std::ios::ate)
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
        return "application/octet-stream"; // Default binary stream
    }

    std::string ext = filePath.substr(dotPos);
    
    // Convert to lowercase to handle extensions like .HTML or .CSS
    for (char& c : ext) {
        c = tolower(c);
    }

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
