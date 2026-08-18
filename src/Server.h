#ifndef SERVER_H
#define SERVER_H

#include <winsock2.h>
#include <string>
#include <vector>
#include <ctime>
#include "Router.h"
#include "Config.h"

// Link with Ws2_32.lib
#pragma comment(lib, "ws2_32.lib")

// Structure to track active client connections and their states
struct ClientConnection {
    SOCKET socket;               // The socket descriptor
    std::string requestBuffer;   // Buffer accumulating raw request data
    time_t lastActiveTime;       // Timestamp of the last activity (for timeouts)
};

class Server {
public:
    // Constructor: accepts Config object containing server parameters
    Server(const Config& config);
    
    // Destructor: ensures resources are cleaned up
    ~Server();

    // Initializes Winsock, creates a TCP socket, binds it to the port, and starts listening
    bool init();

    // Runs the main server loop to accept client connections using select() I/O multiplexing
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
    
    std::vector<ClientConnection> m_clients; // Active client connections

    // Helper to process a fully received request and send the response
    void processRequest(ClientConnection& client);

    // Helper to check for and disconnect clients that have timed out (inactive)
    void checkTimeouts();

    // Helper to set a socket to non-blocking mode
    bool setNonBlocking(SOCKET socket);

    // Helper method to read the file contents
    std::string readFile(const std::string& filePath, bool& found);

    // Helper method to get the correct MIME content type from a file name
    std::string getMimeType(const std::string& filePath);
};

#endif // SERVER_H
