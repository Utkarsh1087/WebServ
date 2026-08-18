#include "Server.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>

// Constructor copies settings from Config
Server::Server(const Config& config) 
    : m_port(config.getPort()), 
      m_root(config.getRoot()), 
      m_index(config.getIndex()), 
      m_maxBodySize(config.getMaxBodySize()),
      m_listenSocket(INVALID_SOCKET), 
      m_isRunning(false) {}

// Destructor cleans up connections
Server::~Server() {
    stop();
}

bool Server::setNonBlocking(SOCKET socket) {
    u_long mode = 1; // 1 = non-blocking, 0 = blocking
    int result = ioctlsocket(socket, FIONBIO, &mode);
    return result != SOCKET_ERROR;
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

    // 3. Set the listen socket to non-blocking mode (Level 5 requirement)
    if (!setNonBlocking(m_listenSocket)) {
        std::cerr << "[ERROR] Failed to set listen socket to non-blocking mode. Error: " << WSAGetLastError() << "\n";
        closesocket(m_listenSocket);
        WSACleanup();
        return false;
    }

    // 4. Bind the socket.
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

    // 5. Start listening.
    result = listen(m_listenSocket, SOMAXCONN);
    if (result == SOCKET_ERROR) {
        std::cerr << "[ERROR] listen failed with error: " << WSAGetLastError() << "\n";
        closesocket(m_listenSocket);
        WSACleanup();
        return false;
    }

    std::cout << "[SUCCESS] Non-blocking listening socket active on port " << m_port << "\n";
    return true;
}

void Server::start() {
    m_isRunning = true;
    std::cout << "[INFO] Server event loop running using select()...\n";

    while (m_isRunning) {
        fd_set readfds;
        FD_ZERO(&readfds);

        // 1. Add the listen socket to the select monitoring set
        FD_SET(m_listenSocket, &readfds);

        // 2. Add all currently active client sockets to the monitoring set
        for (const auto& client : m_clients) {
            FD_SET(client.socket, &readfds);
        }

        // 3. Set a timeout for select() so we don't block indefinitely (allows checking timeouts)
        timeval timeout;
        timeout.tv_sec = 1;  // 1 second
        timeout.tv_usec = 0;

        // select() monitors all specified sockets and blocks until one or more are ready for reading.
        // It returns when an event occurs or when the timeout expires.
        int activity = select(0, &readfds, NULL, NULL, &timeout);

        if (activity == SOCKET_ERROR) {
            if (m_isRunning) {
                std::cerr << "[ERROR] select() failed: " << WSAGetLastError() << "\n";
            }
            break;
        }

        // 4. Check if we have incoming client connections on the listen socket
        if (FD_ISSET(m_listenSocket, &readfds)) {
            // Read in a loop to accept all pending client connections (standard non-blocking accept)
            while (true) {
                SOCKET clientSocket = accept(m_listenSocket, NULL, NULL);
                if (clientSocket == INVALID_SOCKET) {
                    int err = WSAGetLastError();
                    if (err != WSAEWOULDBLOCK) {
                        std::cerr << "[WARNING] accept failed with error: " << err << "\n";
                    }
                    break; // No more clients to accept at this millisecond
                }

                // Set client socket to non-blocking
                if (setNonBlocking(clientSocket)) {
                    ClientConnection newClient = { clientSocket, "", time(NULL) };
                    m_clients.push_back(newClient);
                    std::cout << "[CLIENT CONNECTED] Socket " << clientSocket << " | Active clients: " << m_clients.size() << "\n";
                } else {
                    closesocket(clientSocket);
                }
            }
        }

        // 5. Check active clients for incoming HTTP request data
        for (size_t i = 0; i < m_clients.size(); ) {
            ClientConnection& client = m_clients[i];

            if (FD_ISSET(client.socket, &readfds)) {
                const int bufSize = 4096;
                std::vector<char> buf(bufSize, 0);

                int bytesRead = recv(client.socket, buf.data(), bufSize - 1, 0);

                if (bytesRead > 0) {
                    // Accumulate received data into the client's request buffer
                    client.requestBuffer.append(buf.data(), bytesRead);
                    client.lastActiveTime = time(NULL); // Update liveness timestamp

                    // Check if the request is complete (Level 2/5 state verification)
                    size_t headerEnd = client.requestBuffer.find("\r\n\r\n");
                    if (headerEnd != std::string::npos) {
                        HttpRequest tempReq = HttpParser::parse(client.requestBuffer);
                        
                        if (tempReq.isParsedSuccessfully) {
                            auto lenIt = tempReq.headers.find("content-length");
                            if (lenIt != tempReq.headers.end()) {
                                try {
                                    size_t contentLength = std::stoull(lenIt->second);
                                    
                                    // Security size check
                                    if (contentLength > m_maxBodySize) {
                                        Router::sendResponse(client.socket, 413, "Payload Too Large", "text/plain", "413 Payload Too Large");
                                        closesocket(client.socket);
                                        m_clients.erase(m_clients.begin() + i);
                                        std::cout << "[CLIENT DISCONNECTED] Payload limit hit on socket " << client.socket << "\n";
                                        continue;
                                    }

                                    // If body is fully received, process it
                                    if (tempReq.body.length() >= contentLength) {
                                        processRequest(client);
                                        closesocket(client.socket);
                                        m_clients.erase(m_clients.begin() + i);
                                        continue;
                                    }
                                } catch (...) {
                                    Router::sendResponse(client.socket, 400, "Bad Request", "text/plain", "400 Bad Request");
                                    closesocket(client.socket);
                                    m_clients.erase(m_clients.begin() + i);
                                    continue;
                                }
                            } else {
                                // No Content-Length header, request is fully complete (like standard GET/HEAD)
                                processRequest(client);
                                closesocket(client.socket);
                                m_clients.erase(m_clients.begin() + i);
                                continue;
                            }
                        }
                    }
                } 
                else if (bytesRead == 0) {
                    // Client closed connection gracefully
                    std::cout << "[CLIENT DISCONNECTED] Graceful close on socket " << client.socket << "\n";
                    closesocket(client.socket);
                    m_clients.erase(m_clients.begin() + i);
                    continue;
                } 
                else {
                    int err = WSAGetLastError();
                    if (err != WSAEWOULDBLOCK) {
                        // Hard socket error, clean up
                        std::cout << "[CLIENT DISCONNECTED] Error " << err << " on socket " << client.socket << "\n";
                        closesocket(client.socket);
                        m_clients.erase(m_clients.begin() + i);
                        continue;
                    }
                }
            }
            
            // Move to next client
            i++;
        }

        // 6. Check for inactive connection timeouts (Level 6 requirement)
        checkTimeouts();
    }
}

void Server::checkTimeouts() {
    time_t now = time(NULL);
    const int TIMEOUT_LIMIT_SECONDS = 10; // Timeout after 10 seconds of silence

    for (size_t i = 0; i < m_clients.size(); ) {
        ClientConnection& client = m_clients[i];
        if (now - client.lastActiveTime > TIMEOUT_LIMIT_SECONDS) {
            std::cout << "[TIMEOUT] Closing connection due to inactivity on socket " << client.socket << "\n";
            
            // Send 408 Request Timeout back to the client
            Router::sendResponse(client.socket, 408, "Request Timeout", "text/plain", "408 Request Timeout: Inactive connection closed");
            closesocket(client.socket);

            // Remove client and do not increment loop pointer
            m_clients.erase(m_clients.begin() + i);
        } else {
            i++;
        }
    }
}

void Server::processRequest(ClientConnection& client) {
    HttpRequest request = HttpParser::parse(client.requestBuffer);

    // Security Check: Path Traversal
    if (request.uri.find("..") != std::string::npos) {
        Router::sendResponse(client.socket, 400, "Bad Request", "text/plain", "400 Bad Request: Invalid Path");
        return;
    }

    // Try Dynamic Routing
    if (m_router.route(request, client.socket)) {
        return;
    }

    // Static File Serving (Only allows GET and HEAD)
    if (request.method != "GET" && request.method != "HEAD") {
        Router::sendResponse(client.socket, 405, "Method Not Allowed", "text/plain", "405 Method Not Allowed");
        return;
    }

    // Resolve static path
    std::string uri = request.uri;
    if (uri == "/" || uri.empty()) {
        uri = "/" + m_index;
    }
    std::string localFilePath = m_root + uri;

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
        std::string custom404Content = readFile(m_root + "/404.html", custom404Found);
        
        statusLine = "HTTP/1.1 404 Not Found\r\n";
        mimeType = "text/html; charset=utf-8";
        
        if (custom404Found) {
            responseBody = custom404Content;
        } else {
            responseBody = "<html><body><h1>404 Not Found</h1><p>The requested URL was not found on this server.</p></body></html>";
        }
    }

    // Write headers
    std::ostringstream responseStream;
    responseStream << statusLine
                   << "Content-Type: " << mimeType << "\r\n"
                   << "Content-Length: " << responseBody.length() << "\r\n"
                   << "Connection: close\r\n"
                   << "\r\n";

    std::string headers = responseStream.str();
    send(client.socket, headers.c_str(), (int)headers.length(), 0);

    // Write body (omitted if method is HEAD)
    if (request.method != "HEAD") {
        send(client.socket, responseBody.c_str(), (int)responseBody.length(), 0);
    }
    
    std::cout << "[RESPONSE SENT] Socket " << client.socket << " | Path: " << request.uri 
              << " | Code: " << (fileFound ? "200" : "404") << "\n";
}

void Server::stop() {
    if (m_isRunning) {
        m_isRunning = false;
        std::cout << "[INFO] Stopping server...\n";
    }

    // Close all active client connections
    for (const auto& client : m_clients) {
        closesocket(client.socket);
    }
    m_clients.clear();

    if (m_listenSocket != INVALID_SOCKET) {
        closesocket(m_listenSocket);
        m_listenSocket = INVALID_SOCKET;
    }

    WSACleanup();
    std::cout << "[INFO] Server stopped clean.\n";
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
