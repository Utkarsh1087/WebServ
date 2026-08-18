#include "HttpParser.h"
#include <sstream>
#include <iostream>
#include <algorithm>

HttpRequest HttpParser::parse(const std::string& rawRequest) {
    HttpRequest request;

    // HTTP requests separate headers and body by a double CRLF (\r\n\r\n)
    size_t headerEnd = rawRequest.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        // Double CRLF not found; request might be malformed or incomplete
        return request;
    }

    std::string headerSection = rawRequest.substr(0, headerEnd);
    std::string bodySection = rawRequest.substr(headerEnd + 4);

    // Use a stringstream to parse line by line
    std::istringstream stream(headerSection);
    std::string line;

    // 1. Parse Request Line (First line of the HTTP Request)
    // Format: GET /index.html HTTP/1.1
    if (std::getline(stream, line)) {
        // Strip trailing \r if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::istringstream lineStream(line);
        lineStream >> request.method >> request.uri >> request.version;

        if (request.method.empty() || request.uri.empty() || request.version.empty()) {
            return request; // Invalid request line
        }
    } else {
        return request; // Empty request
    }

    // 2. Parse Headers
    // Format: Header-Name: Value
    while (std::getline(stream, line)) {
        // Strip trailing \r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // Empty line signals the end of the header section (shouldn't happen here as we split by \r\n\r\n, but good safeguard)
        if (line.empty()) {
            break;
        }

        size_t colonPos = line.find(':');
        if (colonPos != std::string::npos) {
            std::string name = toLower(trim(line.substr(0, colonPos)));
            std::string value = trim(line.substr(colonPos + 1));
            request.headers[name] = value;
        }
    }

    // 3. Assign Body
    // If a request has a body (like POST), it must specify Content-Length
    request.body = bodySection;
    request.isParsedSuccessfully = true;

    return request;
}

std::string HttpParser::trim(const std::string& str) {
    if (str.empty()) return "";
    
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

std::string HttpParser::toLower(const std::string& str) {
    std::string lowerStr = str;
    std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(), [](unsigned char c) {
        return std::tolower(c);
    });
    return lowerStr;
}
