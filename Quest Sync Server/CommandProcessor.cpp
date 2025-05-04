#include "CommandProcessor.h"
#include "TCPServer.h"
#include "Logger.h"
#include <algorithm>
#include <iomanip>
#include <Windows.h>

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
    std::cin.clear();
    std::cout.clear();

    // Set the running flag to false
    m_running = false;

    // Notify the input thread to wake up
    m_queueCondition.notify_all();

    try {
        // Create a new thread to generate input to unblock std::getline
        std::thread([this]() {
            try {
                // Generate a newline to unblock std::getline
                // This is a more reliable approach than trying to send EOF
                if (IsConsoleAvailable()) {
                    // Simulate pressing Enter to unblock getline
                    INPUT inputs[2] = {};
                    inputs[0].type = INPUT_KEYBOARD;
                    inputs[0].ki.wVk = VK_RETURN;
                    inputs[1].type = INPUT_KEYBOARD;
                    inputs[1].ki.wVk = VK_RETURN;
                    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
                    SendInput(2, inputs, sizeof(INPUT));
                }
            }
            catch (...) {
                // Ignore any exceptions in this helper thread
            }
        }).detach(); // Detach is safe here as this is just a helper thread

        // Wait for the input thread to finish with a timeout
        if (m_inputThread.joinable()) {
            // Try to reset std::cin to help the input thread exit cleanly
            try {
                // Clear any error flags on cin
                std::cin.clear();

                // Give the thread a chance to exit gracefully
                for (int i = 0; i < 10 && m_inputThread.joinable(); i++) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
            }
            catch (const std::exception& e) {
                LOG_DEBUG("Exception while clearing cin state: " + std::string(e.what()));
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

        while (m_running) {
            // Read a line from the console
            std::string input;
            std::cout << "> " << std::flush; // Use flush to ensure prompt appears immediately

            // Check if we're still running before blocking on getline
            if (!m_running) {
                break;
            }

            try {
                // Check if std::cin is in a good state
                if (std::cin.good() && !std::cin.eof()) {
                    try {
                        // Use getline to read input with a timeout mechanism
                        if (!m_running) {
                            break;
                        }

                        // Use getline to read input
                        std::getline(std::cin, input);

                        // Check if we're still running after getline
                        if (!m_running) {
                            break;
                        }
                    }
                    catch (const std::exception& e) {
                        // If we get an exception while reading and we're shutting down, just exit gracefully
                        if (!m_running) {
                            break;
                        }
                        LOG_ERROR("Exception while reading input: " + std::string(e.what()));
                    }

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
                            LOG_DEBUG("Command processor received shutdown request");

                            // Reset console state to help with clean shutdown
                            std::cin.clear();
                            std::cout.clear();

                            // Exit the input thread loop
                            break;
                        }
                    }
                }
                else {
                    // If cin is in a bad state, sleep a bit to avoid busy waiting
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));

                    // Try to clear the error state
                    if (std::cin.fail()) {
                        std::cin.clear();
                    }
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
