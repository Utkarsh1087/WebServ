#include "Config.h"
#include <iostream>
#include <fstream>
#include <sstream>

Config::Config() 
    : m_port(8080), m_root("./public"), m_index("index.html"), m_maxBodySize(10485760) {} // 10MB Default

bool Config::load(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cout << "[WARNING] Configuration file " << filePath << " not found. Using default settings.\n";
        return false;
    }

    std::string line;
    int lineNum = 0;
    while (std::getline(file, line)) {
        lineNum++;
        line = trim(line);

        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Skip braces for the server block structure
        if (line == "server {" || line == "}") {
            continue;
        }

        // Find the key = value pair
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) {
            std::cerr << "[WARNING] Config syntax error in " << filePath << " line " << lineNum << ": missing '=' delimiter.\n";
            continue;
        }

        std::string key = trim(line.substr(0, eqPos));
        std::string val = trim(line.substr(eqPos + 1));

        // Assign parameters
        if (key == "port") {
            try {
                m_port = std::stoi(val);
            } catch (...) {
                std::cerr << "[ERROR] Invalid port value at line " << lineNum << "\n";
            }
        } else if (key == "root") {
            m_root = val;
            // Clean up surrounding quotes if present (e.g. root = "./public")
            if (m_root.length() >= 2 && m_root.front() == '"' && m_root.back() == '"') {
                m_root = m_root.substr(1, m_root.length() - 2);
            }
        } else if (key == "index") {
            m_index = val;
            if (m_index.length() >= 2 && m_index.front() == '"' && m_index.back() == '"') {
                m_index = m_index.substr(1, m_index.length() - 2);
            }
        } else if (key == "max_body_size") {
            try {
                m_maxBodySize = std::stoull(val);
            } catch (...) {
                std::cerr << "[ERROR] Invalid max_body_size at line " << lineNum << "\n";
            }
        } else {
            std::cerr << "[WARNING] Unknown config parameter: " << key << " at line " << lineNum << "\n";
        }
    }

    std::cout << "[SUCCESS] Loaded config " << filePath << ": Port=" << m_port << ", Root=" << m_root << ", Index=" << m_index << "\n";
    return true;
}

std::string Config::trim(const std::string& str) {
    if (str.empty()) return "";
    size_t first = str.find_first_not_of(" \t\r\n;");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n;");
    return str.substr(first, (last - first + 1));
}
