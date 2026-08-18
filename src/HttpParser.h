#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include <string>
#include <unordered_map>

// Structure representing a parsed HTTP request
struct HttpRequest {
    std::string method;                                  // GET, POST, HEAD, etc.
    std::string uri;                                     // e.g., /index.html
    std::string version;                                 // e.g., HTTP/1.1
    std::unordered_map<std::string, std::string> headers; // Case-insensitive header map
    std::string body;                                    // Request body payload (for POST requests)
    bool isParsedSuccessfully = false;                  // Flag for valid structure
};

class HttpParser {
public:
    // Main method to parse a raw HTTP request string
    static HttpRequest parse(const std::string& rawRequest);

private:
    // Helper to trim leading and trailing whitespaces from strings
    static std::string trim(const std::string& str);

    // Helper to convert a string to lowercase (for case-insensitive header lookups)
    static std::string toLower(const std::string& str);
};

#endif // HTTP_PARSER_H
