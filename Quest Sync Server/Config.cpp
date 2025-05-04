#include "Config.h"
#include "Logger.h"
#include <fstream>
#include <algorithm>
#include <cctype>

// Get the singleton instance
Config& Config::GetInstance() {
    static Config instance;
    return instance;
}

// Constructor
Config::Config() {
    // Initialize with basic default values
    InitializeDefaultConfig();
}

// Initialize with default configuration values
void Config::InitializeDefaultConfig() {
    // Server settings
    m_config["Server.IpAddress"] = "";  // Empty for any address
    m_config["Server.Port"] = "25575";

    // Logging settings
    m_config["Logging.ConsoleLevel"] = "INFO";
    m_config["Logging.FileLevel"] = "DEBUG";
    m_config["Logging.LogFile"] = "Quest Sync Server.log";

    // Connection settings
    m_config["Connection.MaxClients"] = "10";
    m_config["Connection.Timeout"] = "30";  // Seconds

    // Security settings
    m_config["Security.EnableAuthentication"] = "false";
    m_config["Security.AllowedIPs"] = "";  // Empty for all IPs

    // Performance settings
    m_config["Performance.HeartbeatInterval"] = "5";  // Seconds

    // Command Interface settings
    m_config["CommandInterface.Enabled"] = "true";
}

// Load configuration from a file
bool Config::Load(const std::string& filename) {
    // First check if the file exists
    bool fileExists = false;
    {
        std::ifstream checkFile(filename);
        fileExists = checkFile.is_open();
    }

    // If file doesn't exist, create it with default values
    if (!fileExists) {
        LOG_INFO("Config file not found: " + filename + ", creating with default values");

        // Create a temporary config with default values
        std::map<std::string, std::string> defaultConfig;

        // Server settings
        defaultConfig["Server.IpAddress"] = "";  // Empty for any address
        defaultConfig["Server.Port"] = "25575";

        // Logging settings
        defaultConfig["Logging.ConsoleLevel"] = "INFO";
        defaultConfig["Logging.FileLevel"] = "DEBUG";
        defaultConfig["Logging.LogFile"] = "Quest Sync Server.log";

        // Connection settings
        defaultConfig["Connection.MaxClients"] = "10";
        defaultConfig["Connection.Timeout"] = "30";  // Seconds

        // Security settings
        defaultConfig["Security.EnableAuthentication"] = "false";
        defaultConfig["Security.AllowedIPs"] = "";  // Empty for all IPs

        // Performance settings
        defaultConfig["Performance.HeartbeatInterval"] = "5";  // Seconds

        // Command Interface settings
        defaultConfig["CommandInterface.Enabled"] = "true";

        // Write the default config to file
        std::ofstream file(filename);
        if (!file.is_open()) {
            LOG_ERROR("Failed to create default configuration file: " + filename);
            return false;
        }

        // Group configuration by section
        std::map<std::string, std::map<std::string, std::string>> sections;

        for (const auto& pair : defaultConfig) {
            // Split key into section and name
            size_t pos = pair.first.find('.');
            if (pos != std::string::npos) {
                std::string section = pair.first.substr(0, pos);
                std::string name = pair.first.substr(pos + 1);
                sections[section][name] = pair.second;
            } else {
                // No section, use empty string as section name
                sections[""][pair.first] = pair.second;
            }
        }

        // Write each section with comments
        for (const auto& section : sections) {
            // Write section header if not empty
            if (!section.first.empty()) {
                // Add section comments
                if (section.first == "Server") {
                    file << "# Server configuration settings" << std::endl;
                    file << "# These settings control the network behavior of the server" << std::endl;
                }
                else if (section.first == "Logging") {
                    file << "# Logging configuration settings" << std::endl;
                    file << "# Control how the server logs information" << std::endl;
                    file << "# Log levels: DEBUG, INFO, WARNING, ERROR, CRITICAL" << std::endl;
                }
                else if (section.first == "Connection") {
                    file << "# Connection settings" << std::endl;
                    file << "# Control how clients connect to the server" << std::endl;
                }
                else if (section.first == "Security") {
                    file << "# Security settings" << std::endl;
                    file << "# Control authentication and access restrictions" << std::endl;
                }
                else if (section.first == "Performance") {
                    file << "# Performance settings" << std::endl;
                    file << "# Control server performance and resource usage" << std::endl;
                }
                else if (section.first == "CommandInterface") {
                    file << "# Command Interface settings" << std::endl;
                    file << "# Control the server's command-line interface" << std::endl;
                }

                file << "[" << section.first << "]" << std::endl;
            }

            // Write key-value pairs with comments
            for (const auto& pair : section.second) {
                // Add specific comments for each setting
                if (section.first == "Server") {
                    if (pair.first == "IpAddress") {
                        file << "# The IP address to bind to. Leave empty to bind to all available interfaces" << std::endl;
                    }
                    else if (pair.first == "Port") {
                        file << "# The port number the server will listen on" << std::endl;
                    }
                }
                else if (section.first == "Logging") {
                    if (pair.first == "ConsoleLevel") {
                        file << "# The minimum level of messages to display in the console" << std::endl;
                    }
                    else if (pair.first == "FileLevel") {
                        file << "# The minimum level of messages to write to the log file" << std::endl;
                    }
                    else if (pair.first == "LogFile") {
                        file << "# The name of the log file" << std::endl;
                    }
                }
                else if (section.first == "Connection") {
                    if (pair.first == "MaxClients") {
                        file << "# Maximum number of clients that can connect simultaneously" << std::endl;
                    }
                    else if (pair.first == "Timeout") {
                        file << "# Connection timeout in seconds" << std::endl;
                    }
                }
                else if (section.first == "Security") {
                    if (pair.first == "EnableAuthentication") {
                        file << "# Whether to require authentication for clients (true/false)" << std::endl;
                    }
                    else if (pair.first == "AllowedIPs") {
                        file << "# Comma-separated list of allowed IP addresses. Leave empty to allow all" << std::endl;
                    }
                }
                else if (section.first == "Performance") {
                    if (pair.first == "HeartbeatInterval") {
                        file << "# Interval in seconds between heartbeat messages to clients" << std::endl;
                    }
                }
                else if (section.first == "CommandInterface") {
                    if (pair.first == "Enabled") {
                        file << "# Enable or disable the command-line interface (true/false)" << std::endl;
                    }
                }

                file << pair.first << "=" << pair.second << std::endl;
            }

            // Add a blank line between sections
            file << std::endl;
        }

        file.close();
        LOG_INFO("Created default configuration file: " + filename);

        // Now set the config in memory
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_filename = filename;
            m_config = defaultConfig;
        }

        return true;
    }

    // File exists, load it
    std::map<std::string, std::string> loadedConfig;
    std::ifstream file(filename);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open existing configuration file: " + filename);
        return false;
    }

    // Read the config file
    std::string line;
    std::string section;

    while (std::getline(file, line)) {
        // Create a local copy of the line for trimming
        std::string trimmedLine = line;

        // Trim whitespace (without using class method to avoid potential issues)
        trimmedLine.erase(trimmedLine.begin(),
            std::find_if(trimmedLine.begin(), trimmedLine.end(),
                [](unsigned char ch) { return !std::isspace(ch); }));
        trimmedLine.erase(
            std::find_if(trimmedLine.rbegin(), trimmedLine.rend(),
                [](unsigned char ch) { return !std::isspace(ch); }).base(),
            trimmedLine.end());

        // Skip empty lines and comments
        if (trimmedLine.empty() || trimmedLine[0] == ';' || trimmedLine[0] == '#') {
            continue;
        }

        // Check for section header
        if (trimmedLine[0] == '[' && trimmedLine[trimmedLine.length() - 1] == ']') {
            section = trimmedLine.substr(1, trimmedLine.length() - 2);
            continue;
        }

        // Parse key-value pair
        size_t pos = trimmedLine.find('=');
        if (pos != std::string::npos) {
            std::string key = trimmedLine.substr(0, pos);
            std::string value = trimmedLine.substr(pos + 1);

            // Trim whitespace
            key.erase(key.begin(),
                std::find_if(key.begin(), key.end(),
                    [](unsigned char ch) { return !std::isspace(ch); }));
            key.erase(
                std::find_if(key.rbegin(), key.rend(),
                    [](unsigned char ch) { return !std::isspace(ch); }).base(),
                key.end());

            value.erase(value.begin(),
                std::find_if(value.begin(), value.end(),
                    [](unsigned char ch) { return !std::isspace(ch); }));
            value.erase(
                std::find_if(value.rbegin(), value.rend(),
                    [](unsigned char ch) { return !std::isspace(ch); }).base(),
                value.end());

            // Store in config map with section prefix
            if (!section.empty()) {
                key = section + "." + key;
            }

            loadedConfig[key] = value;
        }
    }

    file.close();
    LOG_INFO("Configuration loaded from " + filename);

    // Create a default config to check for missing settings
    std::map<std::string, std::string> defaultConfig;

    // Server settings
    defaultConfig["Server.IpAddress"] = "";  // Empty for any address
    defaultConfig["Server.Port"] = "25575";

    // Logging settings
    defaultConfig["Logging.ConsoleLevel"] = "INFO";
    defaultConfig["Logging.FileLevel"] = "DEBUG";
    defaultConfig["Logging.LogFile"] = "Quest Sync Server.log";

    // Connection settings
    defaultConfig["Connection.MaxClients"] = "10";
    defaultConfig["Connection.Timeout"] = "30";  // Seconds

    // Security settings
    defaultConfig["Security.EnableAuthentication"] = "false";
    defaultConfig["Security.AllowedIPs"] = "";  // Empty for all IPs

    // Performance settings
    defaultConfig["Performance.HeartbeatInterval"] = "5";  // Seconds

    // Check for missing settings and add defaults
    bool configUpdated = false;

    for (const auto& pair : defaultConfig) {
        if (loadedConfig.find(pair.first) == loadedConfig.end()) {
            loadedConfig[pair.first] = pair.second;
            LOG_DEBUG("Added missing config setting: " + pair.first + "=" + pair.second);
            configUpdated = true;
        }
    }

    // Update the config in memory
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_filename = filename;
        m_config = loadedConfig;
    }

    // If config was updated with new default values, save it
    if (configUpdated) {
        LOG_INFO("Configuration updated with new default values");

        // Write the updated config back to file
        std::ofstream outFile(filename);
        if (!outFile.is_open()) {
            LOG_ERROR("Failed to update configuration file with new default values");
            return true; // Still return true as we have the config in memory
        }

        // Group configuration by section
        std::map<std::string, std::map<std::string, std::string>> sections;

        for (const auto& pair : loadedConfig) {
            // Split key into section and name
            size_t pos = pair.first.find('.');
            if (pos != std::string::npos) {
                std::string section = pair.first.substr(0, pos);
                std::string name = pair.first.substr(pos + 1);
                sections[section][name] = pair.second;
            } else {
                // No section, use empty string as section name
                sections[""][pair.first] = pair.second;
            }
        }

        // Write each section with comments
        for (const auto& section : sections) {
            // Write section header if not empty
            if (!section.first.empty()) {
                // Add section comments
                if (section.first == "Server") {
                    outFile << "# Server configuration settings" << std::endl;
                    outFile << "# These settings control the network behavior of the server" << std::endl;
                }
                else if (section.first == "Logging") {
                    outFile << "# Logging configuration settings" << std::endl;
                    outFile << "# Control how the server logs information" << std::endl;
                    outFile << "# Log levels: DEBUG, INFO, WARNING, ERROR, CRITICAL" << std::endl;
                }
                else if (section.first == "Connection") {
                    outFile << "# Connection settings" << std::endl;
                    outFile << "# Control how clients connect to the server" << std::endl;
                }
                else if (section.first == "Security") {
                    outFile << "# Security settings" << std::endl;
                    outFile << "# Control authentication and access restrictions" << std::endl;
                }
                else if (section.first == "Performance") {
                    outFile << "# Performance settings" << std::endl;
                    outFile << "# Control server performance and resource usage" << std::endl;
                }

                outFile << "[" << section.first << "]" << std::endl;
            }

            // Write key-value pairs with comments
            for (const auto& pair : section.second) {
                // Add specific comments for each setting
                if (section.first == "Server") {
                    if (pair.first == "IpAddress") {
                        outFile << "# The IP address to bind to. Leave empty to bind to all available interfaces" << std::endl;
                    }
                    else if (pair.first == "Port") {
                        outFile << "# The port number the server will listen on" << std::endl;
                    }
                }
                else if (section.first == "Logging") {
                    if (pair.first == "ConsoleLevel") {
                        outFile << "# The minimum level of messages to display in the console" << std::endl;
                    }
                    else if (pair.first == "FileLevel") {
                        outFile << "# The minimum level of messages to write to the log file" << std::endl;
                    }
                    else if (pair.first == "LogFile") {
                        outFile << "# The name of the log file" << std::endl;
                    }
                }
                else if (section.first == "Connection") {
                    if (pair.first == "MaxClients") {
                        outFile << "# Maximum number of clients that can connect simultaneously" << std::endl;
                    }
                    else if (pair.first == "Timeout") {
                        outFile << "# Connection timeout in seconds" << std::endl;
                    }
                }
                else if (section.first == "Security") {
                    if (pair.first == "EnableAuthentication") {
                        outFile << "# Whether to require authentication for clients (true/false)" << std::endl;
                    }
                    else if (pair.first == "AllowedIPs") {
                        outFile << "# Comma-separated list of allowed IP addresses. Leave empty to allow all" << std::endl;
                    }
                }
                else if (section.first == "Performance") {
                    if (pair.first == "HeartbeatInterval") {
                        outFile << "# Interval in seconds between heartbeat messages to clients" << std::endl;
                    }
                }

                outFile << pair.first << "=" << pair.second << std::endl;
            }

            // Add a blank line between sections
            outFile << std::endl;
        }

        outFile.close();
    }

    return true;
}

// Save configuration to a file
bool Config::Save(const std::string& filename) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Use the current filename if none is provided
    std::string saveFilename = filename.empty() ? m_filename : filename;
    if (saveFilename.empty()) {
        LOG_ERROR("No filename specified for saving configuration");
        return false;
    }

    // Open the config file for writing
    std::ofstream file(saveFilename);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open config file for writing: " + saveFilename);
        return false;
    }

    // Group configuration by section
    std::map<std::string, std::map<std::string, std::string>> sections;

    for (const auto& pair : m_config) {
        // Split key into section and name
        size_t pos = pair.first.find('.');
        if (pos != std::string::npos) {
            std::string section = pair.first.substr(0, pos);
            std::string name = pair.first.substr(pos + 1);
            sections[section][name] = pair.second;
        } else {
            // No section, use empty string as section name
            sections[""][pair.first] = pair.second;
        }
    }

    // Write each section with comments
    for (const auto& section : sections) {
        // Write section header if not empty
        if (!section.first.empty()) {
            // Add section comments
            if (section.first == "Server") {
                file << "# Server configuration settings" << std::endl;
                file << "# These settings control the network behavior of the server" << std::endl;
            }
            else if (section.first == "Logging") {
                file << "# Logging configuration settings" << std::endl;
                file << "# Control how the server logs information" << std::endl;
                file << "# Log levels: DEBUG, INFO, WARNING, ERROR, CRITICAL" << std::endl;
            }
            else if (section.first == "Connection") {
                file << "# Connection settings" << std::endl;
                file << "# Control how clients connect to the server" << std::endl;
            }
            else if (section.first == "Security") {
                file << "# Security settings" << std::endl;
                file << "# Control authentication and access restrictions" << std::endl;
            }
            else if (section.first == "Performance") {
                file << "# Performance settings" << std::endl;
                file << "# Control server performance and resource usage" << std::endl;
            }
            else if (section.first == "CommandInterface") {
                file << "# Command Interface settings" << std::endl;
                file << "# Control the server's command-line interface" << std::endl;
            }

            file << "[" << section.first << "]" << std::endl;
        }

        // Write key-value pairs with comments
        for (const auto& pair : section.second) {
            // Add specific comments for each setting
            if (section.first == "Server") {
                if (pair.first == "IpAddress") {
                    file << "# The IP address to bind to. Leave empty to bind to all available interfaces" << std::endl;
                }
                else if (pair.first == "Port") {
                    file << "# The port number the server will listen on" << std::endl;
                }
            }
            else if (section.first == "Logging") {
                if (pair.first == "ConsoleLevel") {
                    file << "# The minimum level of messages to display in the console" << std::endl;
                }
                else if (pair.first == "FileLevel") {
                    file << "# The minimum level of messages to write to the log file" << std::endl;
                }
                else if (pair.first == "LogFile") {
                    file << "# The name of the log file" << std::endl;
                }
            }
            else if (section.first == "Connection") {
                if (pair.first == "MaxClients") {
                    file << "# Maximum number of clients that can connect simultaneously" << std::endl;
                }
                else if (pair.first == "Timeout") {
                    file << "# Connection timeout in seconds" << std::endl;
                }
            }
            else if (section.first == "Security") {
                if (pair.first == "EnableAuthentication") {
                    file << "# Whether to require authentication for clients (true/false)" << std::endl;
                }
                else if (pair.first == "AllowedIPs") {
                    file << "# Comma-separated list of allowed IP addresses. Leave empty to allow all" << std::endl;
                }
            }
            else if (section.first == "Performance") {
                if (pair.first == "HeartbeatInterval") {
                    file << "# Interval in seconds between heartbeat messages to clients" << std::endl;
                }
            }

            file << pair.first << "=" << pair.second << std::endl;
        }

        // Add a blank line between sections
        file << std::endl;
    }

    LOG_INFO("Configuration saved to " + saveFilename);
    return true;
}

// Get a configuration value
std::string Config::GetString(const std::string& key, const std::string& defaultValue) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_config.find(key);
    if (it != m_config.end()) {
        return it->second;
    }

    return defaultValue;
}

// Get a configuration value as an integer
int Config::GetInt(const std::string& key, int defaultValue) {
    std::string value = GetString(key, "");

    if (value.empty()) {
        return defaultValue;
    }

    try {
        return std::stoi(value);
    } catch (const std::exception&) {
        LOG_WARNING("Failed to convert config value to integer: " + key + "=" + value + ", using default");
        return defaultValue;
    }
}

// Get a configuration value as a boolean
bool Config::GetBool(const std::string& key, bool defaultValue) {
    std::string value = GetString(key, "");

    if (value.empty()) {
        return defaultValue;
    }

    // Convert to lowercase
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // Check for true values
    if (value == "true" || value == "yes" || value == "1" || value == "on") {
        return true;
    }

    // Check for false values
    if (value == "false" || value == "no" || value == "0" || value == "off") {
        return false;
    }

    LOG_WARNING("Failed to convert config value to boolean: " + key + "=" + value + ", using default");
    return defaultValue;
}

// Set a configuration value
void Config::SetString(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config[key] = value;
}

// Set a configuration value as an integer
void Config::SetInt(const std::string& key, int value) {
    SetString(key, std::to_string(value));
}

// Set a configuration value as a boolean
void Config::SetBool(const std::string& key, bool value) {
    SetString(key, value ? "true" : "false");
}

// Trim whitespace from a string
void Config::Trim(std::string& str) {
    // Trim leading whitespace
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));

    // Trim trailing whitespace
    str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), str.end());
}

// Check if the configuration has all default settings
bool Config::HasAllDefaultSettings() {
    // Create a temporary default config
    std::map<std::string, std::string> defaultConfig;

    // Manually populate the default config with the same values as InitializeDefaultConfig
    // Server settings
    defaultConfig["Server.IpAddress"] = "";  // Empty for any address
    defaultConfig["Server.Port"] = "25575";

    // Logging settings
    defaultConfig["Logging.ConsoleLevel"] = "INFO";
    defaultConfig["Logging.FileLevel"] = "DEBUG";
    defaultConfig["Logging.LogFile"] = "Quest Sync Server.log";

    // Connection settings
    defaultConfig["Connection.MaxClients"] = "10";
    defaultConfig["Connection.Timeout"] = "30";  // Seconds

    // Security settings
    defaultConfig["Security.EnableAuthentication"] = "false";
    defaultConfig["Security.AllowedIPs"] = "";  // Empty for all IPs

    // Performance settings
    defaultConfig["Performance.HeartbeatInterval"] = "5";  // Seconds

    // Command Interface settings
    defaultConfig["CommandInterface.Enabled"] = "true";

    // Get a copy of the current config
    std::map<std::string, std::string> currentConfig;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        currentConfig = m_config;
    }

    // Check if all default settings exist in the current config
    for (const auto& pair : defaultConfig) {
        if (currentConfig.find(pair.first) == currentConfig.end()) {
            return false;
        }
    }

    return true;
}
