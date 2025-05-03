#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <thread>
#include <chrono>
#include <csignal>

#include "TCPServer.h"
#include "Message.h"
#include "Version.h"
#include "Logger.h"
#include "Config.h"
#include "CommandProcessor.h"

// Global server instance for signal handling
TCPServer* g_server = nullptr;
volatile sig_atomic_t g_shutdown = 0;

// Signal handler for graceful shutdown
void SignalHandler(int signal) {
    LOG_INFO("Received signal " + std::to_string(signal) + ", shutting down...");
    g_shutdown = 1;
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

        case MessageType::DATA_REQUEST: {
            // Example: client is requesting data
            std::string request = message.GetPayloadAsString();
            LOG_INFO("Client " + std::to_string(clientSocket) + " requested data: " + request);

            // Create a response
            std::string responseData = "Response to request: " + request;
            Message response(MessageType::DATA_RESPONSE, responseData);
            if (!server->SendToClient(clientSocket, response)) {
                LOG_ERROR("Failed to send data response to client " + std::to_string(clientSocket));
            }
            break;
        }

        case MessageType::EVENT_NOTIFICATION: {
            // Example: client sent an event notification
            std::string event = message.GetPayloadAsString();
            LOG_INFO("Client " + std::to_string(clientSocket) + " sent event: " + event);

            // Broadcast to all other clients
            server->BroadcastMessage(message, clientSocket);
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
        }
        catch (const std::exception& e) {
            LOG_CRITICAL("Exception during configuration loading: " + std::string(e.what()));
            return 1;
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

        // Create and start the command processor if enabled
        CommandProcessor cmdProcessor(&server);
        bool enableCommandInterface = config.GetBool("CommandInterface.Enabled", true);

        if (enableCommandInterface) {
            if (!cmdProcessor.Start()) {
                LOG_WARNING("Failed to start command processor, continuing without CLI");
            }
        } else {
            LOG_INFO("Command interface is disabled in configuration");
        }

        // Main loop
        LOG_INFO("Server running. Type 'help' for available commands or press Ctrl+C to stop.");

        // Keep the main thread alive until the server is stopped or Ctrl+C is pressed
        while (server.IsRunning() && !g_shutdown) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // If we got here due to Ctrl+C, stop the command processor and server
        if (g_shutdown && server.IsRunning()) {
            LOG_INFO("Stopping server due to shutdown signal...");
            if (enableCommandInterface && cmdProcessor.IsRunning()) {
                cmdProcessor.Stop();
            }
            server.Stop();
        }

        LOG_INFO("Server shutdown complete");
        return 0;
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
