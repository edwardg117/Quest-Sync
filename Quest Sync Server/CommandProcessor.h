#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <vector>
#include <iostream>
#include <sstream>
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
#include <condition_variable>

// Forward declaration of the common shutdown function
void ShutdownServer();

// Command processor is always enabled regardless of build mode
#define COMMAND_PROCESSOR_ENABLED 1

// Forward declaration
class TCPServer;

/**
 * @brief Command processor for handling CLI commands
 */
class CommandProcessor {
public:
    /**
     * @brief Constructor
     *
     * @param server Pointer to the TCP server instance
     */
    CommandProcessor(TCPServer* server);

    /**
     * @brief Destructor
     */
    ~CommandProcessor();

    /**
     * @brief Start the command processor
     *
     * @return true if started successfully, false otherwise
     */
    bool Start();

    /**
     * @brief Stop the command processor
     */
    void Stop();

    /**
     * @brief Process a command
     *
     * @param command The command to process
     * @return true if the server should continue running, false if it should stop
     */
    bool ProcessCommand(const std::string& command);

    /**
     * @brief Check if the command processor is running
     *
     * @return true if running, false otherwise
     */
    bool IsRunning() const { return m_running; }

    /**
     * @brief Signal that the command processor can start accepting input
     */
    void SetReadyForInput() { m_readyForInput = true; }

private:
    // Server reference
    TCPServer* m_server;

    // Command processor state
    std::atomic<bool> m_running;
    std::atomic<bool> m_readyForInput;

    // Input thread
    std::thread m_inputThread;

    // Command queue
    std::queue<std::string> m_commandQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCondition;

    // Command handlers
    using CommandHandler = std::function<bool(const std::vector<std::string>&)>;
    std::unordered_map<std::string, CommandHandler> m_commandHandlers;

    /**
     * @brief Input thread function
     */
    void InputThreadFunc();

    /**
     * @brief Register all command handlers
     */
    void RegisterCommands();

    /**
     * @brief Split a string into tokens
     *
     * @param input The input string
     * @return std::vector<std::string> The tokens
     */
    std::vector<std::string> TokenizeCommand(const std::string& input);

    // Command handlers
    bool HandleHelp(const std::vector<std::string>& args);
    bool HandleStop(const std::vector<std::string>& args);
    bool HandleStatus(const std::vector<std::string>& args);

    /**
     * @brief Check if a valid console is available for input/output
     *
     * @return true if a console is available, false otherwise
     */
    bool IsConsoleAvailable();
};
