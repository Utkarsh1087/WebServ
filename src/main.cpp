#include "Server.h"
#include "Config.h"
#include <iostream>
#include <cstdlib>

int main(int argc, char* argv[]) {
    Config config;
    std::string configPath = "webserv.conf"; // Default configuration file

    // Allow passing a custom config file or port number via arguments
    if (argc > 1) {
        std::string arg1 = argv[1];
        
        // If the argument looks like a configuration file (ends with .conf)
        if (arg1.length() >= 5 && arg1.substr(arg1.length() - 5) == ".conf") {
            configPath = arg1;
        } else {
            // Otherwise, treat it as a manual port override for quick testing
            try {
                int overridePort = std::stoi(arg1);
                if (overridePort > 0 && overridePort <= 65535) {
                    std::cout << "[INFO] Overriding port to " << overridePort << " via CLI argument.\n";
                    // Note: We will load the default config, and then we could override it.
                    // We will parse the file first, then override the port.
                }
            } catch (...) {
                std::cerr << "[WARNING] Argument " << arg1 << " is neither a .conf file nor a valid port number.\n";
            }
        }
    }

    // Load the configuration file settings (falls back to defaults if file missing)
    config.load(configPath);

    // If the user specified a manual port override that was not a config path
    if (argc > 1) {
        std::string arg1 = argv[1];
        if (arg1.length() < 5 || arg1.substr(arg1.length() - 5) != ".conf") {
            try {
                int overridePort = std::stoi(arg1);
                if (overridePort > 0 && overridePort <= 65535) {
                    config.setPort(overridePort);
                }
            } catch (...) {}
        }
    }

    // Initialize the server with the loaded configuration
    Server server(config);

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
