#ifndef SERVER_H
#define SERVER_H

#include <winsock2.h>
#include <string>
#include "Router.h"
#include "Config.h"

// Link with Ws2_32.lib
#pragma comment(lib, "ws2_32.lib")

class Server {
public:
    // Constructor: accepts Config object containing server parameters
    Server(const Config& config);
    
    // Destructor: ensures resources are cleaned up
    ~Server();

    // Initializes Winsock, creates a TCP socket, binds it to the port, and starts listening
    bool init();

    // Runs the main server loop to accept client connections
    void start();

    // Stops the server and cleans up Winsock resources
    void stop();

    // Registers a route with the server's router
    void addRoute(const std::string& method, const std::string& path, RouteHandler handler);

private:
    int m_port;                // Port number
    std::string m_root;        // Public document root directory
    std::string m_index;       // Default home index file name
    size_t m_maxBodySize;      // Max body size in bytes
    
    SOCKET m_listenSocket;     // The listening socket
    bool m_isRunning;          // flag indicating whether the server is running
    Router m_router;           // Router for handling dynamic API requests

    // Helper method to process an incoming connection from a client socket
    void handleClient(SOCKET clientSocket);

    // Helper method to read the file contents
    std::string readFile(const std::string& filePath, bool& found);

    // Helper method to get the correct MIME content type from a file name
    std::string getMimeType(const std::string& filePath);
};

#endif // SERVER_H
