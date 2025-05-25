#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <thread>
#include <chrono>
#include <csignal>
#include <sstream>

#include "TCPServer.h"
#include "Message.h"
#include "Version.h"
#include "Logger.h"
#include "Config.h"
#include "CommandProcessor.h"
#include "SessionManager.h"

// Global server instance for signal handling
TCPServer* g_server = nullptr;
volatile sig_atomic_t g_shutdown = 0;

// Signal handler for graceful shutdown
void SignalHandler(int signal) {
    LOG_INFO("Received signal " + std::to_string(signal) + ", shutting down...");

    // Set the shutdown flag to initiate graceful shutdown
    g_shutdown = 1;

    // When Ctrl+C is pressed (SIGINT), we need to handle it specially
    // to avoid issues with std::cin/std::cout in the command processor
    if (signal == SIGINT) {
        // Reset the console state to help with clean shutdown
        std::cin.clear();
        std::cout.clear();
    }
}

// Common shutdown function to ensure consistent behavior
void ShutdownServer() {
    LOG_INFO("Stopping server due to shutdown signal...");

    // Set the shutdown flag to initiate graceful shutdown
    g_shutdown = 1;

    // Reset console state to help with clean shutdown
    std::cin.clear();
    std::cout.clear();

    // Give the main thread a chance to notice the shutdown flag
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

// Trim whitespace from a string
void Trim(std::string& str) {
    // Trim leading whitespace
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));

    // Trim trailing whitespace
    str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), str.end());
}

// This function is replaced by the Config class

// Message handler callback
void HandleMessage(TCPServer* server, SOCKET clientSocket, const Message& message) {
    // Log message info
    LOG_DEBUG("Received message from client " + std::to_string(clientSocket) +
              ", type: " + std::to_string(static_cast<int>(message.GetType())) +
              ", size: " + std::to_string(message.GetPayloadSize()) + " bytes");

    // Handle different message types
    switch (message.GetType()) {
        case MessageType::HEARTBEAT: {
            // Respond with a heartbeat
            Message response(MessageType::HEARTBEAT);
            if (!server->SendToClient(clientSocket, response)) {
                LOG_ERROR("Failed to send heartbeat response to client " + std::to_string(clientSocket));
            }
            break;
        }

        case MessageType::DISCONNECT: {
            // Client is disconnecting gracefully
            LOG_INFO("Client " + std::to_string(clientSocket) + " requested disconnect");
            // The server loop will handle the actual disconnection
            break;
        }

        case MessageType::ERROR_MESSAGE: {
            // Example: client sent an error message
            std::string errorText = message.GetPayloadAsString();
            LOG_INFO("Client " + std::to_string(clientSocket) + " sent error: " + errorText);
            break;
        }

        case MessageType::UPDATE_QUEST: {
            // Quest update received from client
            std::string questData = message.GetPayloadAsString();
            LOG_INFO("Client " + std::to_string(clientSocket) + " sent quest update: " + questData);

            // Parse the key-value pairs for better logging
            std::map<std::string, std::string> questInfo;

            // Split by semicolons instead of newlines
            std::vector<std::string> pairs;
            std::string delimiter = ";";
            size_t pos = 0;
            std::string token;
            std::string str = questData;

            while ((pos = str.find(delimiter)) != std::string::npos) {
                token = str.substr(0, pos);
                pairs.push_back(token);
                str.erase(0, pos + delimiter.length());
            }
            if (!str.empty()) {
                pairs.push_back(str);
            }

            // Process each key-value pair
            for (const auto& pair : pairs) {
                size_t eqPos = pair.find('=');
                if (eqPos != std::string::npos) {
                    std::string key = pair.substr(0, eqPos);
                    std::string value = pair.substr(eqPos + 1);
                    Trim(key);
                    Trim(value);
                    questInfo[key] = value;
                }
            }

            // Create a more detailed log message
            std::string questName = questInfo["Name"];
            std::string questId = questInfo["ID"];
            std::string flags = questInfo["Flags"];
            std::string active = questInfo["active"];
            std::string completed = questInfo["completed"];
            std::string failed = questInfo["failed"];

            LOG_INFO("Quest Update from client " + std::to_string(clientSocket) + ":");
            LOG_INFO("  Quest: " + questName + " (ID: " + questId + ")");
            LOG_INFO("  Flags: " + flags + " (Active: " + active +
                     ", Completed: " + completed + ", Failed: " + failed + ")");

            // Broadcast to all other clients
            server->BroadcastMessage(message, clientSocket);
            break;
        }

        case MessageType::OBJECTIVE_UPDATE: {
            // Objective update received from client
            std::string objectiveData = message.GetPayloadAsString();
            LOG_INFO("Client " + std::to_string(clientSocket) + " sent objective update: " + objectiveData);

            // Parse the key-value pairs for better logging
            std::map<std::string, std::string> objectiveInfo;

            // Split by semicolons instead of newlines
            std::vector<std::string> pairs;
            std::string delimiter = ";";
            size_t pos = 0;
            std::string token;
            std::string str = objectiveData;

            while ((pos = str.find(delimiter)) != std::string::npos) {
                token = str.substr(0, pos);
                pairs.push_back(token);
                str.erase(0, pos + delimiter.length());
            }
            if (!str.empty()) {
                pairs.push_back(str);
            }

            // Process each key-value pair
            for (const auto& pair : pairs) {
                size_t eqPos = pair.find('=');
                if (eqPos != std::string::npos) {
                    std::string key = pair.substr(0, eqPos);
                    std::string value = pair.substr(eqPos + 1);
                    Trim(key);
                    Trim(value);
                    objectiveInfo[key] = value;
                }
            }

            // Create a more detailed log message
            std::string questName = objectiveInfo["Name"];
            std::string questId = objectiveInfo["ID"];
            std::string objectiveId = objectiveInfo["objectiveId"];
            std::string displayText = objectiveInfo["displayText"];
            std::string completed = objectiveInfo["completed"];

            LOG_INFO("Objective Update from client " + std::to_string(clientSocket) + ":");
            LOG_INFO("  Quest: " + questName + " (ID: " + questId + ")");
            LOG_INFO("  Objective: " + objectiveId + " - " + displayText);
            LOG_INFO("  Completed: " + completed);

            // Broadcast to all other clients
            server->BroadcastMessage(message, clientSocket);
            break;
        }

        case MessageType::SESSION_TOKEN_REQUEST: {
            // Handle session token refresh request
            try {
                SessionTokenRequest request = SessionTokenRequest::Deserialize(message.GetPayload());

                // Validate current session token
                SessionManager& sessionManager = SessionManager::GetInstance();
                bool validToken = sessionManager.ValidateSessionToken(clientSocket, request.currentToken);

                SessionTokenResponse response;
                if (validToken) {
                    // Get the client IP from the existing session
                    std::string clientIP = sessionManager.GetClientIP(clientSocket);

                    // Generate new session token with preserved IP
                    Config& config = Config::GetInstance();
                    int tokenExpiry = config.GetInt("Security.SessionTokenExpiry", 3600);
                    std::string newToken = sessionManager.GenerateSessionToken(clientSocket, clientIP, tokenExpiry);

                    response = SessionTokenResponse(true, "Session token refreshed successfully", newToken, tokenExpiry);
                    LOG_INFO("Session token refreshed for client " + std::to_string(clientSocket) + " (IP: " + clientIP + ")");
                } else {
                    response = SessionTokenResponse(false, "Invalid session token", "");
                    LOG_WARNING("Session token refresh failed for client " + std::to_string(clientSocket) + ": invalid token");
                }

                // Send response
                Message responseMsg(MessageType::SESSION_TOKEN_RESPONSE);
                std::vector<uint8_t> payload = response.Serialize();
                responseMsg.SetPayload(payload);

                if (!server->SendToClient(clientSocket, responseMsg)) {
                    LOG_ERROR("Failed to send session token response to client " + std::to_string(clientSocket));
                }
            }
            catch (const std::exception& e) {
                LOG_ERROR("Error processing session token request from client " + std::to_string(clientSocket) + ": " + std::string(e.what()));
            }
            break;
        }

        default: {
            // Unknown message type
            LOG_WARNING("Unknown message type " + std::to_string(static_cast<int>(message.GetType())) +
                       " from client " + std::to_string(clientSocket));
            break;
        }
    }
}

int main(int argc, char* argv[]) {
    try {
        // Initialize the logger
        if (!Logger::GetInstance().Initialize("Quest Sync Server.log")) {
            std::cerr << "Warning: Failed to initialize logger, continuing with console output only" << std::endl;
        }

        // Print banner
        LOG_INFO("==================================");
        LOG_INFO("Quest Sync Server v" + Version::VersionToString(Version::ServerVersion));
        LOG_INFO("==================================");

        // Set up signal handlers for graceful shutdown
        signal(SIGINT, SignalHandler);
        signal(SIGTERM, SignalHandler);

        // Get config instance
        Config& config = Config::GetInstance();
        std::string configFilename = "Quest Sync Server.cfg";

        // Load configuration
        try {
            LOG_INFO("Loading configuration from: " + configFilename);
            if (!config.Load(configFilename)) {
                LOG_ERROR("Failed to load or create configuration file: " + configFilename);
                LOG_ERROR("Please check file permissions and try again");
                return 1;
            }

            // Check if all default settings are present
            if (!config.HasAllDefaultSettings()) {
                LOG_WARNING("Configuration file is missing some default settings");
                LOG_INFO("Missing settings will be added with default values");
            }

            // Validate authentication configuration
            bool authEnabled = config.GetBool("Security.EnableAuthentication", false);
            std::string password = config.GetString("Security.Password", "");

            if (authEnabled && password.empty()) {
                LOG_WARNING("Authentication is enabled but no password is set!");
                LOG_WARNING("Clients will not be able to connect until a password is configured.");
                LOG_WARNING("Use the 'config Security.Password <your_password>' command to set a password.");
            }
        }
        catch (const std::exception& e) {
            LOG_CRITICAL("Exception during configuration loading: " + std::string(e.what()));
            return 1;
        }

        // Check if CommandInterface section exists, but don't modify the file
        // Just log a message if it's missing
        if (config.GetString("CommandInterface.Enabled", "") == "") {
            LOG_INFO("CommandInterface section not found in configuration file");
            LOG_INFO("Using default value: CommandInterface.Enabled=true");
            // We'll use the default value from GetBool below, but won't modify the file
        }

        // Set logger levels from config
        std::string consoleLevel = config.GetString("Logging.ConsoleLevel", "INFO");
        std::string fileLevel = config.GetString("Logging.FileLevel", "DEBUG");

        // Set console log level
        if (consoleLevel == "DEBUG") {
            Logger::GetInstance().SetConsoleLevel(LogLevel::DEBUG);
        } else if (consoleLevel == "INFO") {
            Logger::GetInstance().SetConsoleLevel(LogLevel::INFO);
        } else if (consoleLevel == "WARNING") {
            Logger::GetInstance().SetConsoleLevel(LogLevel::WARNING);
        } else if (consoleLevel == "ERROR") {
            Logger::GetInstance().SetConsoleLevel(LogLevel::ERR);
        } else if (consoleLevel == "CRITICAL") {
            Logger::GetInstance().SetConsoleLevel(LogLevel::CRITICAL);
        }

        // Set file log level
        if (fileLevel == "DEBUG") {
            Logger::GetInstance().SetFileLevel(LogLevel::DEBUG);
        } else if (fileLevel == "INFO") {
            Logger::GetInstance().SetFileLevel(LogLevel::INFO);
        } else if (fileLevel == "WARNING") {
            Logger::GetInstance().SetFileLevel(LogLevel::WARNING);
        } else if (fileLevel == "ERROR") {
            Logger::GetInstance().SetFileLevel(LogLevel::ERR);
        } else if (fileLevel == "CRITICAL") {
            Logger::GetInstance().SetFileLevel(LogLevel::CRITICAL);
        }

        // Get server configuration
        std::string ipAddress;
        int port;

        if (argc >= 3) {
            // Use command line arguments
            ipAddress = argv[1];
            port = std::stoi(argv[2]);

            // Update config with command line values
            config.SetString("Server.IpAddress", ipAddress);
            config.SetInt("Server.Port", port);
            config.Save();
        } else {
            // Load from config
            ipAddress = config.GetString("Server.IpAddress", "");
            port = config.GetInt("Server.Port", 25575);
        }

        // Create the server
        TCPServer server(ipAddress, port, HandleMessage);
        g_server = &server;

        // Initialize the server
        if (!server.Initialize()) {
            LOG_CRITICAL("Failed to initialize server");
            return 1;
        }

        // Start the server
        if (!server.Start()) {
            LOG_CRITICAL("Failed to start server");
            return 1;
        }

        // Create the command processor
        CommandProcessor cmdProcessor(&server);
        bool enableCommandInterface = false;

        try {
            // Check if command interface is enabled in config
            enableCommandInterface = config.GetBool("CommandInterface.Enabled", true);

            if (enableCommandInterface) {
                LOG_INFO("Command interface is enabled in configuration");
                if (!cmdProcessor.Start()) {
                    LOG_WARNING("Failed to start command processor, continuing without CLI");
                    enableCommandInterface = false;
                }
            } else {
                LOG_INFO("Command interface is disabled in configuration");
            }
        }
        catch (const std::exception& e) {
            LOG_ERROR("Exception while starting command processor: " + std::string(e.what()));
            LOG_WARNING("Continuing without command interface");
            enableCommandInterface = false;
        }
        catch (...) {
            LOG_ERROR("Unknown exception while starting command processor");
            LOG_WARNING("Continuing without command interface");
            enableCommandInterface = false;
        }

        // Main loop
        if (enableCommandInterface) {
            LOG_INFO("Server running. Type 'help' for available commands or press Ctrl+C to stop.");
            // Signal the command processor that it can start accepting input
            cmdProcessor.SetReadyForInput();
        } else {
            LOG_INFO("Server running. Press Ctrl+C to stop.");
        }

        // Keep the main thread alive until the server is stopped or Ctrl+C is pressed
        while (server.IsRunning() && !g_shutdown) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // If we got here due to Ctrl+C or server stopped, clean up
        LOG_INFO("Stopping server...");

        try {
            // First, set the shutdown flag to prevent new connections
            g_shutdown = 1;

            // Stop the command processor first if it's running
            if (enableCommandInterface && cmdProcessor.IsRunning()) {
                try {
                    // Reset console state to help with clean shutdown
                    std::cin.clear();
                    std::cout.clear();

                    LOG_DEBUG("Stopping command processor...");
                    cmdProcessor.Stop();
                }
                catch (const std::exception& e) {
                    // Don't log errors during normal shutdown as they can be confusing
                    LOG_DEBUG("Exception while stopping command processor: " + std::string(e.what()));
                }
                catch (...) {
                    LOG_DEBUG("Unknown exception while stopping command processor");
                }
            }

            // Give the command processor a moment to clean up
            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            // Stop the server if it's still running
            if (server.IsRunning()) {
                try {
                    LOG_DEBUG("Stopping server...");
                    server.Stop();
                }
                catch (const std::exception& e) {
                    LOG_ERROR("Exception while stopping server: " + std::string(e.what()));
                }
                catch (...) {
                    LOG_ERROR("Unknown exception while stopping server");
                }
            }
        }
        catch (const std::exception& e) {
            LOG_ERROR("Exception during shutdown: " + std::string(e.what()));
        }
        catch (...) {
            LOG_ERROR("Unknown exception during shutdown");
        }

        // Make sure all streams are in a good state before exiting
        std::cin.clear();
        std::cout.clear();

        LOG_INFO("Server shutdown complete");

        // Always exit with code 0 for normal shutdown
        // This ensures we don't return the MessageType::DISCONNECT value (3)
        std::exit(0);
        return 0; // This line will never be reached, but keeps the compiler happy
    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception in main: " << e.what() << std::endl;
        LOG_CRITICAL("Unhandled exception in main: " + std::string(e.what()));
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception in main" << std::endl;
        LOG_CRITICAL("Unknown exception in main");
        return 1;
    }
}






