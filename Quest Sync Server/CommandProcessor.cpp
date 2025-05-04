#include "CommandProcessor.h"
#include "TCPServer.h"
#include "Logger.h"
#include "Config.h"
#include <algorithm>
#include <iomanip>
#include <Windows.h>
#include <conio.h> // For _getch()

// Constructor
CommandProcessor::CommandProcessor(TCPServer* server)
    : m_server(server), m_running(false), m_readyForInput(false) {
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

    try {
        // Set the running flag
        m_running = true;

        // Check if we have a valid console
        if (!IsConsoleAvailable()) {
            LOG_WARNING("No valid console available for command input");
            LOG_INFO("Command processor will be disabled");
            m_running = false;
            return false;
        }

        // Start the input thread
        m_inputThread = std::thread(&CommandProcessor::InputThreadFunc, this);

        LOG_INFO("Command processor started");

        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Failed to start command processor: " + std::string(e.what()));
        m_running = false;
        return false;
    }
    catch (...) {
        LOG_ERROR("Unknown exception while starting command processor");
        m_running = false;
        return false;
    }
}

// Stop the command processor
void CommandProcessor::Stop() {
    if (!m_running) {
        return;
    }

    LOG_DEBUG("Stopping command processor...");

    // Reset console state to help with clean shutdown
    std::cout.clear();

    // Set the running flag to false
    m_running = false;

    // Notify the input thread to wake up
    m_queueCondition.notify_all();

    try {
        // Wait for the input thread to finish with a timeout
        if (m_inputThread.joinable()) {
            // Give the thread a chance to exit gracefully
            for (int i = 0; i < 10 && m_inputThread.joinable(); i++) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            // If the thread is still running, we'll have to detach it
            if (m_inputThread.joinable()) {
                // Don't log a warning during normal shutdown
                LOG_DEBUG("Command processor input thread detaching...");
                m_inputThread.detach();
            }
            else {
                LOG_DEBUG("Input thread exited gracefully");
            }
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception while stopping command processor: " + std::string(e.what()));
    }
    catch (...) {
        LOG_ERROR("Unknown exception while stopping command processor");
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
        // Check for partial command match
        std::vector<std::string> completions = GetCommandCompletions(commandName);

        if (completions.size() == 1) {
            // If there's only one completion, use it
            std::string fullCommand = completions[0];

            // Replace the first token with the full command
            tokens[0] = fullCommand;

            // Find the command handler for the full command
            auto fullIt = m_commandHandlers.find(fullCommand);
            if (fullIt != m_commandHandlers.end()) {
                // Execute the command handler
                return fullIt->second(tokens);
            }
        } else if (completions.size() > 1) {
            // If there are multiple completions, show them
            std::cout << "Ambiguous command: " << tokens[0] << std::endl;
            std::cout << "Did you mean one of these?" << std::endl;
            for (const auto& completion : completions) {
                std::cout << "  " << completion << std::endl;
            }
            return true;
        }

        // Unknown command
        std::cout << "Unknown command: " << tokens[0] << std::endl;
        std::cout << "Type 'help' for a list of available commands." << std::endl;
        return true;
    }
}

// Input thread function
void CommandProcessor::InputThreadFunc() {
    try {
        // Wait until the main thread signals we're ready to display the prompt
        while (m_running && !m_readyForInput) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        // If we're no longer running, exit
        if (!m_running) {
            return;
        }

        // Display help message once at the start
        std::cout << "Type 'help' for a list of available commands." << std::endl;

        // Current input buffer and cursor position
        std::string inputBuffer;

        // Special key codes
        const int KEY_BACKSPACE = 8;
        const int KEY_ENTER = 13;
        const int KEY_ESCAPE = 27;
        const int KEY_TAB = 9;
        const int KEY_CTRL_C = 3;

        // Display the prompt
        std::cout << "> " << std::flush;

        while (m_running) {
            // Check if we're still running
            if (!m_running) {
                break;
            }

            try {
                // Read a character from the console
                int ch = _getch();

                // Handle special keys
                switch (ch) {
                    case KEY_ENTER:
                        // Process the command
                        std::cout << std::endl;

                        if (!inputBuffer.empty()) {
                            // Add the command to the queue
                            {
                                std::lock_guard<std::mutex> lock(m_queueMutex);
                                m_commandQueue.push(inputBuffer);
                            }

                            // Notify the main thread
                            m_queueCondition.notify_one();

                            // Process the command directly in this thread
                            bool continueRunning = ProcessCommand(inputBuffer);

                            // Clear the input buffer for the next command
                            inputBuffer.clear();

                            if (!continueRunning) {
                                // Command requested server shutdown
                                LOG_DEBUG("Command processor received shutdown request");

                                // Exit the input thread loop
                                break;
                            }
                        }

                        // Display the prompt for the next command
                        std::cout << "> " << std::flush;
                        break;

                    case KEY_BACKSPACE:
                        // Handle backspace
                        if (!inputBuffer.empty()) {
                            inputBuffer.pop_back();
                            std::cout << "\b \b" << std::flush; // Erase the character
                        }
                        break;

                    case KEY_TAB:
                        // Handle tab completion
                        if (!inputBuffer.empty()) {
                            // Get completions for the current input
                            std::vector<std::string> completions = GetCommandCompletions(inputBuffer);

                            if (completions.size() == 1) {
                                // If there's only one completion, use it
                                // Clear the current line
                                std::cout << "\r> " << std::string(inputBuffer.length(), ' ') << "\r> " << std::flush;

                                // Update the input buffer
                                inputBuffer = completions[0] + " ";

                                // Display the new input
                                std::cout << inputBuffer << std::flush;
                            } else if (completions.size() > 1) {
                                // If there are multiple completions, show them
                                std::cout << std::endl;

                                // Find the common prefix
                                std::string commonPrefix = completions[0];
                                for (size_t i = 1; i < completions.size(); ++i) {
                                    size_t j = 0;
                                    while (j < commonPrefix.length() && j < completions[i].length() &&
                                           commonPrefix[j] == completions[i][j]) {
                                        ++j;
                                    }
                                    commonPrefix = commonPrefix.substr(0, j);
                                }

                                // If we have a common prefix longer than the current input, use it
                                if (commonPrefix.length() > inputBuffer.length()) {
                                    inputBuffer = commonPrefix;
                                }

                                // Display the completions
                                for (const auto& completion : completions) {
                                    std::cout << completion << "  ";
                                }

                                // Display the prompt and current input
                                std::cout << std::endl << "> " << inputBuffer << std::flush;
                            }
                        }
                        break;

                    case KEY_CTRL_C:
                        // Handle Ctrl+C
                        std::cout << "^C" << std::endl;

                        // Call the stop command
                        ProcessCommand("stop");

                        // Exit the input thread loop
                        break;

                    case KEY_ESCAPE:
                        // Handle escape - clear the current input
                        std::cout << "\r> " << std::string(inputBuffer.length(), ' ') << "\r> " << std::flush;
                        inputBuffer.clear();
                        break;

                    default:
                        // Regular character - add to input buffer and echo
                        if (ch >= 32 && ch <= 126) { // Printable ASCII
                            inputBuffer += static_cast<char>(ch);
                            std::cout << static_cast<char>(ch) << std::flush;
                        }
                        break;
                }
            }
            catch (const std::exception& e) {
                LOG_ERROR("Exception while reading input: " + std::string(e.what()));
                // Sleep a bit to avoid busy waiting in case of repeated exceptions
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            catch (...) {
                LOG_ERROR("Unknown exception while reading input");
                // Sleep a bit to avoid busy waiting in case of repeated exceptions
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception in command processor input thread: " + std::string(e.what()));
    }
    catch (...) {
        LOG_ERROR("Unknown exception in command processor input thread");
    }

    LOG_DEBUG("Command processor input thread exiting");
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

// Handle the help command
bool CommandProcessor::HandleHelp(const std::vector<std::string>& args) {
    // If a specific command was specified, show detailed help for that command
    if (args.size() > 1) {
        std::string commandName = args[1];
        std::transform(commandName.begin(), commandName.end(), commandName.begin(),
                      [](unsigned char c) { return std::tolower(c); });

        // Check if it's an alias
        auto aliasIt = m_commandAliases.find(commandName);
        if (aliasIt != m_commandAliases.end()) {
            commandName = aliasIt->second;
        }

        // Find the command info
        auto infoIt = m_commandInfo.find(commandName);
        if (infoIt != m_commandInfo.end()) {
            const CommandInfo& info = infoIt->second;

            std::cout << "Command: " << info.name << std::endl;
            std::cout << "Description: " << info.description << std::endl;

            // Show aliases if any
            if (!info.aliases.empty()) {
                std::cout << "Aliases: ";
                for (size_t i = 0; i < info.aliases.size(); ++i) {
                    std::cout << info.aliases[i];
                    if (i < info.aliases.size() - 1) {
                        std::cout << ", ";
                    }
                }
                std::cout << std::endl;
            }

            // Show usage based on the command
            std::cout << "Usage: " << info.name;
            if (info.name == "help") {
                std::cout << " [command]";
            } else if (info.name == "kick") {
                std::cout << " <client_id>";
            } else if (info.name == "broadcast") {
                std::cout << " <message>";
            } else if (info.name == "config") {
                std::cout << " [key] [value]";
            }
            std::cout << std::endl;

            return true;
        } else {
            std::cout << "Unknown command: " << args[1] << std::endl;
            return true;
        }
    }

    // Group commands by category
    std::map<CommandCategory, std::vector<const CommandInfo*>> commandsByCategory;

    for (const auto& pair : m_commandInfo) {
        commandsByCategory[pair.second.category].push_back(&pair.second);
    }

    // Display commands by category
    std::cout << "Available commands:" << std::endl;

    // General commands
    if (commandsByCategory.find(CommandCategory::GENERAL) != commandsByCategory.end()) {
        std::cout << "General:" << std::endl;
        for (const CommandInfo* info : commandsByCategory[CommandCategory::GENERAL]) {
            std::cout << "  " << std::left << std::setw(10) << info->name << " - " << info->description << std::endl;
        }
    }

    // Server commands
    if (commandsByCategory.find(CommandCategory::SERVER) != commandsByCategory.end()) {
        std::cout << "Server:" << std::endl;
        for (const CommandInfo* info : commandsByCategory[CommandCategory::SERVER]) {
            std::cout << "  " << std::left << std::setw(10) << info->name << " - " << info->description << std::endl;
        }
    }

    // Client commands
    if (commandsByCategory.find(CommandCategory::CLIENT) != commandsByCategory.end()) {
        std::cout << "Client:" << std::endl;
        for (const CommandInfo* info : commandsByCategory[CommandCategory::CLIENT]) {
            std::cout << "  " << std::left << std::setw(10) << info->name << " - " << info->description << std::endl;
        }
    }

    // Config commands
    if (commandsByCategory.find(CommandCategory::CONFIG) != commandsByCategory.end()) {
        std::cout << "Configuration:" << std::endl;
        for (const CommandInfo* info : commandsByCategory[CommandCategory::CONFIG]) {
            std::cout << "  " << std::left << std::setw(10) << info->name << " - " << info->description << std::endl;
        }
    }

    std::cout << std::endl;
    std::cout << "Type 'help <command>' for more information about a specific command." << std::endl;

    return true;
}

// Handle the stop command
bool CommandProcessor::HandleStop(const std::vector<std::string>& args) {
    std::cout << "Stopping server..." << std::endl;

    try {
        // Reset console state to help with clean shutdown
        std::cin.clear();
        std::cout.clear();

        // Call the common shutdown function to ensure consistent behavior with Ctrl+C
        ShutdownServer();

        // Set the running flag to false to stop the input thread
        m_running = false;

        // Give the main thread a chance to notice the shutdown flag
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    catch (const std::exception& e) {
        LOG_DEBUG("Exception in stop command: " + std::string(e.what()));
    }
    catch (...) {
        LOG_DEBUG("Unknown exception in stop command");
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

// Handle the clients command
bool CommandProcessor::HandleClients(const std::vector<std::string>& args) {
    if (!m_server) {
        std::cout << "Server not available" << std::endl;
        return true;
    }

    // Get client information
    auto clientInfo = m_server->GetClientInfo();

    if (clientInfo.empty()) {
        std::cout << "No clients connected" << std::endl;
        return true;
    }

    // Display client information
    std::cout << "Connected clients: " << clientInfo.size() << std::endl;
    std::cout << std::left << std::setw(10) << "ID" << std::setw(25) << "IP Address" << "Status" << std::endl;
    std::cout << std::string(50, '-') << std::endl;

    for (const auto& client : clientInfo) {
        SOCKET socket = std::get<0>(client);
        std::string ipAddress = std::get<1>(client);
        bool authenticated = std::get<2>(client);

        std::cout << std::left << std::setw(10) << socket << std::setw(25) << ipAddress
                  << (authenticated ? "Authenticated" : "Not authenticated") << std::endl;
    }

    return true;
}

// Handle the kick command
bool CommandProcessor::HandleKick(const std::vector<std::string>& args) {
    if (!m_server) {
        std::cout << "Server not available" << std::endl;
        return true;
    }

    // Check if a client ID was specified
    if (args.size() < 2) {
        std::cout << "Usage: kick <client_id>" << std::endl;
        return true;
    }

    // Parse the client ID
    SOCKET clientSocket;
    try {
        clientSocket = static_cast<SOCKET>(std::stoi(args[1]));
    } catch (const std::exception&) {
        std::cout << "Invalid client ID: " << args[1] << std::endl;
        return true;
    }

    // Kick the client
    if (m_server->KickClient(clientSocket)) {
        std::cout << "Client " << clientSocket << " has been disconnected" << std::endl;
    } else {
        std::cout << "Client " << clientSocket << " not found" << std::endl;
    }

    return true;
}

// Handle the broadcast command
bool CommandProcessor::HandleBroadcast(const std::vector<std::string>& args) {
    if (!m_server) {
        std::cout << "Server not available" << std::endl;
        return true;
    }

    // Check if a message was specified
    if (args.size() < 2) {
        std::cout << "Usage: broadcast <message>" << std::endl;
        return true;
    }

    // Combine all arguments after the command into a single message
    std::string message;
    for (size_t i = 1; i < args.size(); ++i) {
        message += args[i];
        if (i < args.size() - 1) {
            message += " ";
        }
    }

    // Broadcast the message
    m_server->BroadcastText(message);

    std::cout << "Message broadcast to all clients: " << message << std::endl;

    return true;
}

// Handle the config command
bool CommandProcessor::HandleConfig(const std::vector<std::string>& args) {
    Config& config = Config::GetInstance();

    // If no arguments, display all configuration settings
    if (args.size() == 1) {
        std::cout << "Configuration settings:" << std::endl;

        // Server settings
        std::cout << "Server:" << std::endl;
        std::cout << "  IpAddress = " << config.GetString("Server.IpAddress", "") << std::endl;
        std::cout << "  Port = " << config.GetInt("Server.Port", 25575) << std::endl;

        // Logging settings
        std::cout << "Logging:" << std::endl;
        std::cout << "  ConsoleLevel = " << config.GetString("Logging.ConsoleLevel", "INFO") << std::endl;
        std::cout << "  FileLevel = " << config.GetString("Logging.FileLevel", "DEBUG") << std::endl;
        std::cout << "  LogFile = " << config.GetString("Logging.LogFile", "Quest Sync Server.log") << std::endl;

        // Connection settings
        std::cout << "Connection:" << std::endl;
        std::cout << "  MaxClients = " << config.GetInt("Connection.MaxClients", 10) << std::endl;
        std::cout << "  Timeout = " << config.GetInt("Connection.Timeout", 30) << std::endl;

        // Security settings
        std::cout << "Security:" << std::endl;
        std::cout << "  EnableAuthentication = " << config.GetBool("Security.EnableAuthentication", false) << std::endl;
        std::cout << "  AllowedIPs = " << config.GetString("Security.AllowedIPs", "") << std::endl;

        // Performance settings
        std::cout << "Performance:" << std::endl;
        std::cout << "  HeartbeatInterval = " << config.GetInt("Performance.HeartbeatInterval", 5) << std::endl;

        return true;
    }

    // If one argument, display the value of that setting
    if (args.size() == 2) {
        std::string key = args[1];

        // Try to get the value
        std::string value = config.GetString(key, "");

        if (value.empty()) {
            std::cout << "Configuration setting not found: " << key << std::endl;
        } else {
            std::cout << key << " = " << value << std::endl;
        }

        return true;
    }

    // If two arguments, set the value of that setting
    if (args.size() >= 3) {
        std::string key = args[1];
        std::string value = args[2];

        // Set the value
        config.SetString(key, value);

        // Save the configuration
        if (config.Save()) {
            std::cout << "Configuration setting updated: " << key << " = " << value << std::endl;
        } else {
            std::cout << "Failed to save configuration" << std::endl;
        }

        return true;
    }

    return true;
}

// Check if a valid console is available for input/output
bool CommandProcessor::IsConsoleAvailable() {
    try {
        // Get standard input/output handles
        HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
        HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);

        // Check if handles are valid
        if (hStdin == INVALID_HANDLE_VALUE || hStdout == INVALID_HANDLE_VALUE) {
            LOG_WARNING("Invalid console handles");
            return false;
        }

        // Check if handles are console handles
        DWORD mode;
        if (!GetConsoleMode(hStdin, &mode) || !GetConsoleMode(hStdout, &mode)) {
            LOG_WARNING("Console mode not available");
            return false;
        }

        // Check if std::cin and std::cout are in a good state
        if (!std::cin.good() || !std::cout.good()) {
            // Only log as debug since this can happen during normal shutdown with Ctrl+C
            LOG_DEBUG("std::cin or std::cout is in a bad state");

            // Try to reset the state
            std::cin.clear();
            std::cout.clear();

            // Still return false if we can't recover
            if (!std::cin.good() || !std::cout.good()) {
                return false;
            }
        }

        return true;
    }
    catch (const std::exception& e) {
        LOG_ERROR("Exception while checking console availability: " + std::string(e.what()));
        return false;
    }
    catch (...) {
        LOG_ERROR("Unknown exception while checking console availability");
        return false;
    }
}
