#ifndef CONFIG_H
#define CONFIG_H

#include <string>

class Config {
public:
    // Constructor sets default configuration values
    Config();
    
    // Loads and parses the configuration file (e.g. webserv.conf)
    // Returns true if successfully loaded, false if not (retaining defaults)
    bool load(const std::string& filePath);

    // Accessors for config properties
    int getPort() const { return m_port; }
    void setPort(int port) { m_port = port; }
    const std::string& getRoot() const { return m_root; }
    const std::string& getIndex() const { return m_index; }
    size_t getMaxBodySize() const { return m_maxBodySize; }

private:
    int m_port;
    std::string m_root;
    std::string m_index;
    size_t m_maxBodySize;

    // Helper to strip whitespace
    std::string trim(const std::string& str);
};

#endif // CONFIG_H
