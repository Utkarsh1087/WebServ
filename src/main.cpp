#include "Server.h"
#include <iostream>
#include <cstdlib>

int main(int argc, char* argv[]) {
    // Default port is 8080
    int port = 8080;

    // Allow user to override port via command line argument
    if (argc > 1) {
        port = std::atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            std::cerr << "[ERROR] Invalid port number. Must be between 1 and 65535.\n";
            return 1;
        }
    }

    // Create the server instance
    Server server(port);

    // --- Dynamic API Route Registration (Levels 2 & 3) ---

    // 1. GET /api/health - Returns a JSON status check
    server.addRoute("GET", "/api/health", [](const HttpRequest& req, SOCKET clientSocket) {
        std::string jsonBody = "{\"status\":\"healthy\",\"engine\":\"C++ WebServ\",\"version\":\"1.0.0\"}";
        Router::sendResponse(clientSocket, 200, "OK", "application/json; charset=utf-8", jsonBody);
    });

    // 2. POST /api/echo - Echoes back the body payload posted by the client
    server.addRoute("POST", "/api/echo", [](const HttpRequest& req, SOCKET clientSocket) {
        if (req.body.empty()) {
            Router::sendResponse(clientSocket, 400, "Bad Request", "text/plain", "400 Bad Request: Missing body content");
        } else {
            Router::sendResponse(clientSocket, 200, "OK", "text/plain; charset=utf-8", "Echo received: " + req.body);
        }
    });

    // 3. HEAD /api/health - Test route for HEAD request behaviors on dynamic paths
    server.addRoute("HEAD", "/api/health", [](const HttpRequest& req, SOCKET clientSocket) {
        // HEAD requests only write headers, no body
        Router::sendResponse(clientSocket, 200, "OK", "application/json; charset=utf-8", "");
    });

    // Initialize sockets, bind, and listen
    if (!server.init()) {
        std::cerr << "[ERROR] Failed to initialize server.\n";
        return 1;
    }

    // Start the accept loop
    server.start();

    return 0;
}
