#include "Router.h"
#include <iostream>
#include <sstream>
#include <algorithm>

void Router::addRoute(const std::string& method, const std::string& path, RouteHandler handler) {
    std::string uppercaseMethod = method;
    std::transform(uppercaseMethod.begin(), uppercaseMethod.end(), uppercaseMethod.begin(), ::toupper);
    
    std::string key = getRouteKey(uppercaseMethod, path);
    m_routes[key] = handler;
    std::cout << "[ROUTE REGISTERED] " << uppercaseMethod << " " << path << "\n";
}

bool Router::route(const HttpRequest& request, SOCKET clientSocket) {
    std::string key = getRouteKey(request.method, request.uri);
    
    auto it = m_routes.find(key);
    if (it != m_routes.end()) {
        // Match found! Execute the callback handler
        it->second(request, clientSocket);
        return true;
    }
    
    // No matching route found
    return false;
}

std::string Router::getRouteKey(const std::string& method, const std::string& path) const {
    return method + ":" + path;
}

void Router::sendResponse(SOCKET clientSocket, int statusCode, const std::string& statusText, 
                           const std::string& contentType, const std::string& body) {
    std::ostringstream responseStream;
    responseStream << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n"
                   << "Content-Type: " << contentType << "\r\n"
                   << "Content-Length: " << body.length() << "\r\n"
                   << "Connection: close\r\n"
                   << "\r\n" // Crucial header/body separator
                   << body;

    std::string fullResponse = responseStream.str();
    send(clientSocket, fullResponse.c_str(), (int)fullResponse.length(), 0);
}
