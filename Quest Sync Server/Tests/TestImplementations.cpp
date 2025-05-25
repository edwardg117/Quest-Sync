#include "Message.h"
#include "Logger.h"
#include "Config.h"
#include "TCPServer.h"
#include "CommandProcessor.h"
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <algorithm>
#include <conio.h> // For _getch() mock

// Add missing member variable for test implementation
namespace {
    // This is a test-only extension of the Logger class to add the m_logFilePath member
    // that's needed for the test implementation but not in the actual Logger class
    std::string g_logFilePath;

    // Mock for _getch() to use in tests
    int g_nextChar = 13; // Default to Enter key
}

// Mock implementation of _getch() for testing
int _getch() {
    return g_nextChar;
}

// Message class implementations
Message::Message() : m_header() {}
Message::Message(MessageType type) : m_header(type, 0) {}
Message::Message(MessageType type, const std::vector<uint8_t>& payload)
    : m_header(type, static_cast<uint32_t>(payload.size())), m_payload(payload) {}
Message::Message(MessageType type, const std::string& payload)
    : m_header(type, static_cast<uint32_t>(payload.size())) {
    m_payload.resize(payload.size());
    if (!payload.empty()) {
        std::copy(payload.begin(), payload.end(), m_payload.begin());
    }
}

// Constructor with type and session token
Message::Message(MessageType type, const std::string& sessionToken, bool isSessionToken)
    : m_header(type, 0, static_cast<uint32_t>(sessionToken.size())), m_sessionToken(sessionToken) {
    // isSessionToken parameter is used to distinguish this constructor from the string payload constructor
}

std::string Message::GetPayloadAsString() const {
    return std::string(reinterpret_cast<const char*>(m_payload.data()), m_payload.size());
}

void Message::SetPayload(const std::vector<uint8_t>& payload) {
    m_payload = payload;
    m_header.payloadSize = static_cast<uint32_t>(payload.size());
}

void Message::SetPayload(const std::string& payload) {
    m_payload.resize(payload.size());
    if (!payload.empty()) {
        std::copy(payload.begin(), payload.end(), m_payload.begin());
    }
    m_header.payloadSize = static_cast<uint32_t>(payload.size());
}

// Session token methods
void Message::SetSessionToken(const std::string& token) {
    m_sessionToken = token;
    m_header.sessionTokenLength = static_cast<uint32_t>(token.size());
}

void Message::ClearSessionToken() {
    m_sessionToken.clear();
    m_header.sessionTokenLength = 0;
}

std::vector<uint8_t> Message::Serialize() const {
    // Calculate total size: header + session token + payload
    size_t totalSize = sizeof(MessageHeader) + m_sessionToken.size() + m_payload.size();
    std::vector<uint8_t> result(totalSize);

    size_t offset = 0;

    // Copy header
    std::memcpy(result.data() + offset, &m_header, sizeof(MessageHeader));
    offset += sizeof(MessageHeader);

    // Copy session token if present
    if (!m_sessionToken.empty()) {
        std::memcpy(result.data() + offset, m_sessionToken.data(), m_sessionToken.size());
        offset += m_sessionToken.size();
    }

    // Copy payload
    if (!m_payload.empty()) {
        std::memcpy(result.data() + offset, m_payload.data(), m_payload.size());
    }

    return result;
}

std::unique_ptr<Message> Message::Deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(MessageHeader)) {
        return nullptr;
    }

    // Extract header
    MessageHeader header;
    std::memcpy(&header, data.data(), sizeof(MessageHeader));

    // Check if we have enough data for session token and payload
    size_t requiredSize = sizeof(MessageHeader) + header.sessionTokenLength + header.payloadSize;
    if (data.size() < requiredSize) {
        return nullptr;
    }

    // Create a new message
    auto message = std::make_unique<Message>(header.type);
    message->m_header = header;

    size_t offset = sizeof(MessageHeader);

    // Extract session token if present
    if (header.sessionTokenLength > 0) {
        message->m_sessionToken = std::string(reinterpret_cast<const char*>(data.data() + offset), header.sessionTokenLength);
        offset += header.sessionTokenLength;
    }

    // Extract payload if present
    if (header.payloadSize > 0) {
        message->m_payload.resize(header.payloadSize);
        std::memcpy(message->m_payload.data(), data.data() + offset, header.payloadSize);
    }

    return message;
}

// HandshakeRequest implementations
std::vector<uint8_t> HandshakeRequest::Serialize() const {
    // Calculate size: version (2 ints) + password length + password data
    size_t totalSize = sizeof(int) * 2 + sizeof(uint32_t) + password.size();
    std::vector<uint8_t> result(totalSize);

    // Store version components
    int major = clientVersion[0];
    int minor = clientVersion[1];

    std::memcpy(result.data(), &major, sizeof(int));
    std::memcpy(result.data() + sizeof(int), &minor, sizeof(int));

    // Store password length
    uint32_t passwordLength = static_cast<uint32_t>(password.size());
    std::memcpy(result.data() + sizeof(int) * 2, &passwordLength, sizeof(uint32_t));

    // Store password content
    if (!password.empty()) {
        std::copy(password.begin(), password.end(), result.begin() + sizeof(int) * 2 + sizeof(uint32_t));
    }

    return result;
}

HandshakeRequest HandshakeRequest::Deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(int) * 2 + sizeof(uint32_t)) {
        // For backward compatibility, if we only have version data, create request without password
        if (data.size() >= sizeof(int) * 2) {
            int major, minor;
            std::memcpy(&major, data.data(), sizeof(int));
            std::memcpy(&minor, data.data() + sizeof(int), sizeof(int));
            HandshakeRequest request(std::array<int, 2>{major, minor});
            return request;
        }
        throw std::runtime_error("Not enough data to deserialize HandshakeRequest");
    }

    // Extract version components
    int major, minor;
    std::memcpy(&major, data.data(), sizeof(int));
    std::memcpy(&minor, data.data() + sizeof(int), sizeof(int));

    // Extract password length
    uint32_t passwordLength;
    std::memcpy(&passwordLength, data.data() + sizeof(int) * 2, sizeof(uint32_t));

    // Check if we have enough data for the password
    if (data.size() < sizeof(int) * 2 + sizeof(uint32_t) + passwordLength) {
        throw std::runtime_error("Not enough data to deserialize HandshakeRequest password");
    }

    // Extract password content
    std::string password;
    if (passwordLength > 0) {
        password = std::string(reinterpret_cast<const char*>(data.data() + sizeof(int) * 2 + sizeof(uint32_t)), passwordLength);
    }

    return HandshakeRequest(std::array<int, 2>{major, minor}, password);
}

// HandshakeResponse implementations
std::vector<uint8_t> HandshakeResponse::Serialize() const {
    // Calculate size: bool + message length + message data + token length + token data + expiry seconds
    size_t totalSize = sizeof(bool) + sizeof(uint32_t) + message.size() + sizeof(uint32_t) + sessionToken.size() + sizeof(int);
    std::vector<uint8_t> result(totalSize);

    size_t offset = 0;

    // Store accepted flag
    std::memcpy(result.data() + offset, &accepted, sizeof(bool));
    offset += sizeof(bool);

    // Store message length
    uint32_t messageLength = static_cast<uint32_t>(message.size());
    std::memcpy(result.data() + offset, &messageLength, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Store message content
    if (!message.empty()) {
        std::memcpy(result.data() + offset, message.data(), message.size());
        offset += message.size();
    }

    // Store session token length
    uint32_t tokenLength = static_cast<uint32_t>(sessionToken.size());
    std::memcpy(result.data() + offset, &tokenLength, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Store session token content
    if (!sessionToken.empty()) {
        std::memcpy(result.data() + offset, sessionToken.data(), sessionToken.size());
        offset += sessionToken.size();
    }

    // Store expiry seconds
    std::memcpy(result.data() + offset, &expirySeconds, sizeof(int));

    return result;
}

HandshakeResponse HandshakeResponse::Deserialize(const std::vector<uint8_t>& data) {
    if (data.size() < sizeof(bool) + sizeof(uint32_t)) {
        throw std::runtime_error("Not enough data to deserialize HandshakeResponse");
    }

    size_t offset = 0;

    // Extract accepted flag
    bool accepted;
    std::memcpy(&accepted, data.data() + offset, sizeof(bool));
    offset += sizeof(bool);

    // Extract message length
    uint32_t messageLength;
    std::memcpy(&messageLength, data.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Check if we have enough data for the message
    if (data.size() < offset + messageLength) {
        throw std::runtime_error("Not enough data to deserialize HandshakeResponse message");
    }

    // Extract message content
    std::string message;
    if (messageLength > 0) {
        message = std::string(reinterpret_cast<const char*>(data.data() + offset), messageLength);
        offset += messageLength;
    }

    // Check if we have enough data for session token length
    if (data.size() < offset + sizeof(uint32_t)) {
        throw std::runtime_error("Not enough data to deserialize HandshakeResponse session token length");
    }

    // Extract session token length
    uint32_t tokenLength;
    std::memcpy(&tokenLength, data.data() + offset, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    // Check if we have enough data for session token and expiry
    if (data.size() < offset + tokenLength + sizeof(int)) {
        throw std::runtime_error("Not enough data to deserialize HandshakeResponse session token and expiry");
    }

    // Extract session token content
    std::string sessionToken;
    if (tokenLength > 0) {
        sessionToken = std::string(reinterpret_cast<const char*>(data.data() + offset), tokenLength);
        offset += tokenLength;
    }

    // Extract expiry seconds
    int expirySeconds;
    std::memcpy(&expirySeconds, data.data() + offset, sizeof(int));

    HandshakeResponse response(accepted, message, sessionToken, expirySeconds);
    return response;
}

// Logger class implementations
Logger::Logger() : m_consoleLevel(LogLevel::INFO), m_fileLevel(LogLevel::DEBUG), m_initialized(false), m_mutex() {
    // Empty constructor for testing
}

Logger::~Logger() {
    // Empty destructor for testing
    Close();
}

Logger& Logger::GetInstance() {
    static Logger instance;
    return instance;
}

bool Logger::Initialize(const std::string& logFilePath, LogLevel consoleLevel, LogLevel fileLevel) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_consoleLevel = consoleLevel;
    m_fileLevel = fileLevel;
    g_logFilePath = logFilePath;
    m_initialized = true;

    // Log initialization message
    std::ofstream file(g_logFilePath, std::ios::app);
    if (file.is_open()) {
        file << GetTimestamp() << " [INFO] Logger initialized" << std::endl;
        file.close();
    }

    return true;
}

void Logger::Debug(const std::string& message) {
    Log(LogLevel::DEBUG, message);
}

void Logger::Info(const std::string& message) {
    Log(LogLevel::INFO, message);
}

void Logger::Warning(const std::string& message) {
    Log(LogLevel::WARNING, message);
}

void Logger::Error(const std::string& message) {
    Log(LogLevel::ERR, message);
}

void Logger::Critical(const std::string& message) {
    Log(LogLevel::CRITICAL, message);
}

void Logger::Log(LogLevel level, const std::string& message) {
    // For testing, we need to ensure thread safety
    static std::mutex fileMutex;

    // Check if we should log this message
    bool shouldLog = false;
    LogLevel fileLevel;
    bool isInitialized = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        isInitialized = m_initialized;
        fileLevel = m_fileLevel;
        shouldLog = isInitialized && level >= fileLevel;
    }

    if (!shouldLog) {
        return;
    }

    // Format the log message outside the lock
    std::string timestamp = GetTimestamp();
    std::string levelStr = LogLevelToString(level);
    std::string formattedMessage = timestamp + " [" + levelStr + "] " + message + "\n";

    // Use a separate mutex for file operations to avoid holding the main mutex during I/O
    std::lock_guard<std::mutex> fileLock(fileMutex);

    try {
        // For testing, just write to a file
        std::ofstream file(g_logFilePath, std::ios::app | std::ios::binary);
        if (file.is_open()) {
            file.write(formattedMessage.c_str(), formattedMessage.length());
            file.flush();  // Ensure data is written immediately
            file.close();
        }
    }
    catch (const std::exception&) {
        // Ignore exceptions in test implementation
    }
}

void Logger::Close() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_initialized) {
        // Log closing message
        std::ofstream file(g_logFilePath, std::ios::app);
        if (file.is_open()) {
            file << GetTimestamp() << " [INFO] Logger closed" << std::endl;
            file.close();
        }

        m_initialized = false;
    }
}

void Logger::SetConsoleLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_consoleLevel = level;
}

void Logger::SetFileLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_fileLevel = level;
}

std::string Logger::LogLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERR: return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

std::string Logger::GetTimestamp() {
    return "2023-01-01 12:00:00"; // Fixed timestamp for testing
}

// Config class implementations
Config::Config() : m_mutex() {
    // Empty constructor for testing
    InitializeDefaultConfig();
}

Config& Config::GetInstance() {
    static Config instance;
    return instance;
}

// Initialize with default configuration values
void Config::InitializeDefaultConfig() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Create default config
    m_config["Server.IpAddress"] = "0.0.0.0";  // Changed from empty to explicit any address
    m_config["Server.Port"] = "25575";
    m_config["Logging.ConsoleLevel"] = "INFO";
    m_config["Logging.FileLevel"] = "DEBUG";
    m_config["Logging.LogFile"] = "server.log";
    m_config["Connection.MaxClients"] = "10";
    m_config["Connection.Timeout"] = "30";
    m_config["Security.EnableAuthentication"] = "false";
    m_config["Security.AllowedIPs"] = "";
    m_config["Performance.HeartbeatInterval"] = "5";
    m_config["CommandInterface.Enabled"] = "true";
}

bool Config::Load(const std::string& filename) {
    // Store the filename
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_filename = filename;
    }

    // Check if file exists
    bool fileExists = false;
    {
        std::ifstream checkFile(filename);
        fileExists = checkFile.good();
    }

    // If file doesn't exist, create it with default values
    if (!fileExists) {
        // Create a temporary config with default values
        std::map<std::string, std::string> defaultConfig;

        // Server settings
        defaultConfig["Server.IpAddress"] = "0.0.0.0";  // Changed from empty to explicit any address
        defaultConfig["Server.Port"] = "25575";
        defaultConfig["Logging.ConsoleLevel"] = "INFO";
        defaultConfig["Logging.FileLevel"] = "DEBUG";
        defaultConfig["Logging.LogFile"] = "server.log";
        defaultConfig["Connection.MaxClients"] = "10";
        defaultConfig["Connection.Timeout"] = "30";
        defaultConfig["Security.EnableAuthentication"] = "false";
        defaultConfig["Security.AllowedIPs"] = "";
        defaultConfig["Performance.HeartbeatInterval"] = "5";
        defaultConfig["CommandInterface.Enabled"] = "true";

        // Create the file
        std::ofstream file(filename);
        if (file.is_open()) {
            for (const auto& pair : defaultConfig) {
                file << pair.first << "=" << pair.second << std::endl;
            }
            file.close();
        }

        // Now set the config in memory
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_config = defaultConfig;
        }

        return true;
    }

    // File exists, load it
    std::map<std::string, std::string> loadedConfig;
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    // Read the config file
    std::string line;
    while (std::getline(file, line)) {
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            loadedConfig[key] = value;
        }
    }
    file.close();

    // Create a default config to check for missing settings
    std::map<std::string, std::string> defaultConfig;

    // Server settings
    defaultConfig["Server.IpAddress"] = "";
    defaultConfig["Server.Port"] = "25575";
    defaultConfig["Logging.ConsoleLevel"] = "INFO";
    defaultConfig["Logging.FileLevel"] = "DEBUG";
    defaultConfig["Logging.LogFile"] = "server.log";
    defaultConfig["Connection.MaxClients"] = "10";
    defaultConfig["Connection.Timeout"] = "30";
    defaultConfig["Security.EnableAuthentication"] = "false";
    defaultConfig["Security.AllowedIPs"] = "";
    defaultConfig["Performance.HeartbeatInterval"] = "5";
    defaultConfig["CommandInterface.Enabled"] = "true";

    // Check for missing settings and add defaults
    bool configUpdated = false;

    for (const auto& pair : defaultConfig) {
        if (loadedConfig.find(pair.first) == loadedConfig.end()) {
            loadedConfig[pair.first] = pair.second;
            configUpdated = true;
        }
    }

    // Update the config in memory
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config = loadedConfig;
    }

    // If we added missing settings, save the updated config
    if (configUpdated) {
        Save();
    }

    return true;
}

bool Config::Save(const std::string& filename) {
    std::string saveFilename;
    std::map<std::string, std::string> configCopy;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        saveFilename = filename.empty() ? m_filename : filename;
        configCopy = m_config; // Make a copy to avoid holding the lock during file I/O
    }

    if (saveFilename.empty()) {
        return false;
    }

    std::ofstream file(saveFilename);
    if (!file.is_open()) {
        return false;
    }

    for (const auto& pair : configCopy) {
        file << pair.first << "=" << pair.second << std::endl;
    }

    file.close();
    return true;
}

std::string Config::GetString(const std::string& key, const std::string& defaultValue) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_config.find(key);
    if (it != m_config.end()) {
        return it->second;
    }
    return defaultValue;
}

int Config::GetInt(const std::string& key, int defaultValue) {
    std::string value = GetString(key, "");
    if (value.empty()) {
        return defaultValue;
    }

    try {
        return std::stoi(value);
    } catch (...) {
        return defaultValue;
    }
}

bool Config::GetBool(const std::string& key, bool defaultValue) {
    std::string value = GetString(key, "");
    if (value.empty()) {
        return defaultValue;
    }

    if (value == "true" || value == "1" || value == "yes") {
        return true;
    } else if (value == "false" || value == "0" || value == "no") {
        return false;
    }

    return defaultValue;
}

void Config::SetString(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config[key] = value;
}

void Config::SetInt(const std::string& key, int value) {
    SetString(key, std::to_string(value));
}

void Config::SetBool(const std::string& key, bool value) {
    SetString(key, value ? "true" : "false");
}

bool Config::HasAllDefaultSettings() {
    // Create a temporary default config
    std::map<std::string, std::string> defaultConfig;

    // Server settings
    defaultConfig["Server.IpAddress"] = "";
    defaultConfig["Server.Port"] = "25575";
    defaultConfig["Logging.ConsoleLevel"] = "INFO";
    defaultConfig["Logging.FileLevel"] = "DEBUG";
    defaultConfig["Logging.LogFile"] = "server.log";
    defaultConfig["Connection.MaxClients"] = "10";
    defaultConfig["Connection.Timeout"] = "30";
    defaultConfig["Security.EnableAuthentication"] = "false";
    defaultConfig["Security.AllowedIPs"] = "";
    defaultConfig["Performance.HeartbeatInterval"] = "5";
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

// TCPServer class implementations
TCPServer::TCPServer(const std::string& ipAddress, int port, MessageHandler messageHandler)
    : m_ipAddress(ipAddress), m_port(port), m_messageHandler(messageHandler), m_running(false), m_listenSocket(INVALID_SOCKET) {
}

TCPServer::~TCPServer() {
    Stop();
}

bool TCPServer::Initialize() {
    // For testing, simulate successful initialization
    m_listenSocket = 1; // Set to a valid socket value for testing
    return true;
}

bool TCPServer::Start() {
    // Check if server was initialized first
    if (m_listenSocket == INVALID_SOCKET) {
        return false;
    }
    m_running = true;
    return true;
}

void TCPServer::Stop() {
    m_running = false;
}

bool TCPServer::SendToClient(SOCKET clientSocket, const Message& message) {
    // For testing, just return true
    return true;
}

void TCPServer::BroadcastMessage(const Message& message, SOCKET excludeSocket) {
    // For testing, do nothing
}

void TCPServer::BroadcastText(const std::string& text, SOCKET excludeSocket) {
    // For testing, do nothing
}

std::vector<std::tuple<SOCKET, std::string, bool>> TCPServer::GetClientInfo() {
    // For testing, return empty client list initially
    std::vector<std::tuple<SOCKET, std::string, bool>> clients;
    return clients;
}

bool TCPServer::KickClient(SOCKET clientSocket) {
    // For testing, only succeed for client ID 1
    return clientSocket == 1;
}

size_t TCPServer::GetClientCount() {
    return 0; // For testing, start with no clients
}

// CommandProcessor class implementations
CommandProcessor::CommandProcessor(TCPServer* server)
    : m_server(server), m_running(false), m_readyForInput(false) {
    RegisterCommands();
}

CommandProcessor::~CommandProcessor() {
    Stop();
}

bool CommandProcessor::Start() {
    m_running = true;
    return true;
}

void CommandProcessor::Stop() {
    m_running = false;
}

// IsRunning and SetReadyForInput are already defined as inline functions in CommandProcessor.h

bool CommandProcessor::ProcessCommand(const std::string& command) {
    // Tokenize the command
    std::vector<std::string> tokens = TokenizeCommand(command);

    // If no tokens, return
    if (tokens.empty()) {
        return true;
    }

    // Get the command name (first token)
    std::string commandName = tokens[0];

    // Convert to lowercase
    std::transform(commandName.begin(), commandName.end(), commandName.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // Check if it's an alias
    auto aliasIt = m_commandAliases.find(commandName);
    if (aliasIt != m_commandAliases.end()) {
        // Replace the command name with the actual command
        commandName = aliasIt->second;
    }

    // Find the command handler
    auto it = m_commandHandlers.find(commandName);
    if (it != m_commandHandlers.end()) {
        // Execute the command handler
        return it->second(tokens);
    } else {
        // Unknown command
        return true;
    }
}

// Register all command handlers
void CommandProcessor::RegisterCommands() {
    // Clear existing command handlers and info
    m_commandHandlers.clear();
    m_commandAliases.clear();
    m_commandInfo.clear();

    // Register general commands

    // Help command
    m_commandHandlers["help"] = [this](const std::vector<std::string>& args) {
        return HandleHelp(args);
    };
    m_commandInfo["help"] = {"help", "Display help information", CommandCategory::GENERAL, {}};

    // Stop command
    m_commandHandlers["stop"] = [this](const std::vector<std::string>& args) {
        return HandleStop(args);
    };
    m_commandInfo["stop"] = {"stop", "Stop the server", CommandCategory::SERVER, {"exit", "quit"}};
    m_commandAliases["exit"] = "stop";
    m_commandAliases["quit"] = "stop";

    // Status command
    m_commandHandlers["status"] = [this](const std::vector<std::string>& args) {
        return HandleStatus(args);
    };
    m_commandInfo["status"] = {"status", "Display server status", CommandCategory::SERVER, {}};

    // Register client commands

    // Clients command
    m_commandHandlers["clients"] = [this](const std::vector<std::string>& args) {
        return HandleClients(args);
    };
    m_commandInfo["clients"] = {"clients", "List all connected clients", CommandCategory::CLIENT, {"list"}};
    m_commandAliases["list"] = "clients";

    // Kick command
    m_commandHandlers["kick"] = [this](const std::vector<std::string>& args) {
        return HandleKick(args);
    };
    m_commandInfo["kick"] = {"kick", "Disconnect a specific client", CommandCategory::CLIENT, {"disconnect"}};
    m_commandAliases["disconnect"] = "kick";

    // Broadcast command
    m_commandHandlers["broadcast"] = [this](const std::vector<std::string>& args) {
        return HandleBroadcast(args);
    };
    m_commandInfo["broadcast"] = {"broadcast", "Send a message to all clients", CommandCategory::CLIENT, {"say"}};
    m_commandAliases["say"] = "broadcast";

    // Register config commands

    // Config command
    m_commandHandlers["config"] = [this](const std::vector<std::string>& args) {
        return HandleConfig(args);
    };
    m_commandInfo["config"] = {"config", "View or change configuration settings", CommandCategory::CONFIG, {"settings"}};
    m_commandAliases["settings"] = "config";
}

// Split a string into tokens
std::vector<std::string> CommandProcessor::TokenizeCommand(const std::string& input) {
    std::vector<std::string> tokens;
    std::istringstream iss(input);
    std::string token;

    while (iss >> token) {
        tokens.push_back(token);
    }

    return tokens;
}

// Get possible command completions
std::vector<std::string> CommandProcessor::GetCommandCompletions(const std::string& partial) {
    std::vector<std::string> completions;

    // Convert partial to lowercase for case-insensitive matching
    std::string partialLower = partial;
    std::transform(partialLower.begin(), partialLower.end(), partialLower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // Check all commands
    for (const auto& cmd : m_commandInfo) {
        if (cmd.first.find(partialLower) == 0) {
            completions.push_back(cmd.first);
        }
    }

    // Check all aliases
    for (const auto& alias : m_commandAliases) {
        if (alias.first.find(partialLower) == 0) {
            completions.push_back(alias.first);
        }
    }

    return completions;
}

// Command handlers
bool CommandProcessor::HandleHelp(const std::vector<std::string>& args) {
    return true;
}

bool CommandProcessor::HandleStop(const std::vector<std::string>& args) {
    return false; // Signal to stop
}

bool CommandProcessor::HandleStatus(const std::vector<std::string>& args) {
    return true;
}

bool CommandProcessor::HandleClients(const std::vector<std::string>& args) {
    return true;
}

bool CommandProcessor::HandleKick(const std::vector<std::string>& args) {
    return true;
}

bool CommandProcessor::HandleBroadcast(const std::vector<std::string>& args) {
    return true;
}

bool CommandProcessor::HandleConfig(const std::vector<std::string>& args) {
    return true;
}

bool CommandProcessor::IsConsoleAvailable() {
    return true;
}
