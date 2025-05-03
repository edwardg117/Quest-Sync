#include "CommandProcessor.h"
#include "TCPServer.h"
#include "Logger.h"
#include <algorithm>
#include <iomanip>

// Constructor
CommandProcessor::CommandProcessor(TCPServer* server)
    : m_server(server), m_running(false) {
    // Register command handlers
    RegisterCommands();
}

// Destructor
CommandProcessor::~CommandProcessor() {
    // Stop the command processor if it's running
    if (m_running) {
        Stop();
    }
}

// Start the command processor
bool CommandProcessor::Start() {
    if (m_running) {
        LOG_WARNING("Command processor is already running");
        return false;
    }

    // Set the running flag
    m_running = true;

    // Start the input thread
    m_inputThread = std::thread(&CommandProcessor::InputThreadFunc, this);

    LOG_INFO("Command processor started");
    std::cout << "Type 'help' for a list of available commands." << std::endl;

    return true;
}

// Stop the command processor
void CommandProcessor::Stop() {
    if (!m_running) {
        return;
    }

    // Set the running flag to false
    m_running = false;

    // Notify the input thread to wake up
    m_queueCondition.notify_all();

    // Wait for the input thread to exit
    if (m_inputThread.joinable()) {
        m_inputThread.join();
    }

    LOG_INFO("Command processor stopped");
}

// Process a command
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

    // Find the command handler
    auto it = m_commandHandlers.find(commandName);
    if (it != m_commandHandlers.end()) {
        // Execute the command handler
        return it->second(tokens);
    } else {
        // Unknown command
        std::cout << "Unknown command: " << commandName << std::endl;
        std::cout << "Type 'help' for a list of available commands." << std::endl;
        return true;
    }
}

// Input thread function
void CommandProcessor::InputThreadFunc() {
    while (m_running) {
        // Read a line from the console
        std::string input;
        std::cout << "> ";
        std::getline(std::cin, input);

        // If we got input, process it
        if (!input.empty()) {
            // Add the command to the queue
            {
                std::lock_guard<std::mutex> lock(m_queueMutex);
                m_commandQueue.push(input);
            }

            // Notify the main thread
            m_queueCondition.notify_one();

            // Process the command directly in this thread
            bool continueRunning = ProcessCommand(input);
            if (!continueRunning) {
                // Command requested server shutdown
                break;
            }
        }
    }
}

// Register all command handlers
void CommandProcessor::RegisterCommands() {
    // Register the help command
    m_commandHandlers["help"] = [this](const std::vector<std::string>& args) {
        return HandleHelp(args);
    };

    // Register the stop command
    m_commandHandlers["stop"] = [this](const std::vector<std::string>& args) {
        return HandleStop(args);
    };

    // Register the status command
    m_commandHandlers["status"] = [this](const std::vector<std::string>& args) {
        return HandleStatus(args);
    };
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

// Handle the help command
bool CommandProcessor::HandleHelp(const std::vector<std::string>& args) {
    std::cout << "Available commands:" << std::endl;
    std::cout << "  help    - Display this help message" << std::endl;
    std::cout << "  stop    - Stop the server" << std::endl;
    std::cout << "  status  - Display server status" << std::endl;
    return true;
}

// Handle the stop command
bool CommandProcessor::HandleStop(const std::vector<std::string>& args) {
    std::cout << "Stopping server..." << std::endl;
    
    // Stop the server
    if (m_server) {
        m_server->Stop();
    }
    
    // Return false to indicate the server should stop
    return false;
}

// Handle the status command
bool CommandProcessor::HandleStatus(const std::vector<std::string>& args) {
    if (!m_server) {
        std::cout << "Server not available" << std::endl;
        return true;
    }

    // Get the number of connected clients
    size_t clientCount = m_server->GetClientCount();

    std::cout << "Server status:" << std::endl;
    std::cout << "  Running: " << (m_server->IsRunning() ? "Yes" : "No") << std::endl;
    std::cout << "  Connected clients: " << clientCount << std::endl;
    
    return true;
}
