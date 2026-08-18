#ifndef ROUTER_H
#define ROUTER_H

#include "HttpParser.h"
#include <winsock2.h>
#include <string>
#include <unordered_map>
#include <functional>

// Define a type for route handler callbacks
// It accepts the parsed HttpRequest and the client socket to write responses to.
using RouteHandler = std::function<void(const HttpRequest&, SOCKET)>;

class Router {
public:
    // Register a new route with a method (GET/POST/HEAD) and path (e.g. "/api/health")
    void addRoute(const std::string& method, const std::string& path, RouteHandler handler);

    // Attempts to route the request. Returns true if a match was found and handled, false otherwise.
    bool route(const HttpRequest& request, SOCKET clientSocket);

    // Static helper to quickly send standard HTTP response headers and body
    static void sendResponse(SOCKET clientSocket, int statusCode, const std::string& statusText, 
                             const std::string& contentType, const std::string& body);

private:
    // Helper to generate a unique key combining method and path (e.g., "GET:/api/health")
    std::string getRouteKey(const std::string& method, const std::string& path) const;

    // Maps "METHOD:PATH" key to its associated RouteHandler callback function
    std::unordered_map<std::string, RouteHandler> m_routes;
};

#endif // ROUTER_H
