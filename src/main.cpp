#include "Server.h"
#include <iostream>
#include <cstdlib>

int main(int argc, char* argv[]) {
    // Default port is 8080
    int port = 8080;

    // Allow user to override port via command line argument (e.g. webserv.exe 9000)
    if (argc > 1) {
        port = std::atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            std::cerr << "[ERROR] Invalid port number. Must be between 1 and 65535.\n";
            return 1;
        }
    }

    // Create the server instance
    Server server(port);

    // Initialize sockets, bind, and listen
    if (!server.init()) {
        std::cerr << "[ERROR] Failed to initialize server.\n";
        return 1;
    }

    // Start the accept loop (this blocks until the program is terminated)
    server.start();

    return 0;
}
