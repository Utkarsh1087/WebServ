#ifndef SERVER_H
#define SERVER_H

#include <winsock2.h>
#include <string>
#include "Router.h"

// Link with Ws2_32.lib
#pragma comment(lib, "ws2_32.lib")

class Server {
public:
    // Constructor: sets up the server port
    Server(int port);
    
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
    int m_port;                // The port number to listen on
    SOCKET m_listenSocket;     // The listening socket file descriptor
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
