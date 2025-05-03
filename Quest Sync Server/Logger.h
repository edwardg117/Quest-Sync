#pragma once
#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <iostream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>

/**
 * @brief Log levels for the logging system
 */
enum class LogLevel {
    DEBUG,      // Detailed information for debugging
    INFO,       // General information about system operation
    WARNING,    // Potential issues that don't prevent operation
    ERR,        // Errors that prevent specific operations
    CRITICAL    // Critical errors that prevent system operation
};

/**
 * @brief Logger class for handling application logging
 *
 * This class provides a thread-safe logging system that can output to both
 * console and file with different log levels and timestamps.
 */
class Logger {
public:
    /**
     * @brief Get the singleton instance of the logger
     *
     * @return Reference to the logger instance
     */
    static Logger& GetInstance();

    /**
     * @brief Initialize the logger
     *
     * @param logFilePath Path to the log file
     * @param consoleLevel Minimum level to log to console
     * @param fileLevel Minimum level to log to file
     * @return true if initialization was successful, false otherwise
     */
    bool Initialize(const std::string& logFilePath,
                   LogLevel consoleLevel = LogLevel::INFO,
                   LogLevel fileLevel = LogLevel::DEBUG);

    /**
     * @brief Log a message
     *
     * @param level Log level of the message
     * @param message Message to log
     */
    void Log(LogLevel level, const std::string& message);

    /**
     * @brief Log a debug message
     *
     * @param message Message to log
     */
    void Debug(const std::string& message);

    /**
     * @brief Log an info message
     *
     * @param message Message to log
     */
    void Info(const std::string& message);

    /**
     * @brief Log a warning message
     *
     * @param message Message to log
     */
    void Warning(const std::string& message);

    /**
     * @brief Log an error message
     *
     * @param message Message to log
     */
    void Error(const std::string& message);

    /**
     * @brief Log a critical message
     *
     * @param message Message to log
     */
    void Critical(const std::string& message);

    /**
     * @brief Set the console log level
     *
     * @param level Minimum level to log to console
     */
    void SetConsoleLevel(LogLevel level);

    /**
     * @brief Set the file log level
     *
     * @param level Minimum level to log to file
     */
    void SetFileLevel(LogLevel level);

    /**
     * @brief Close the logger
     */
    void Close();

private:
    // Private constructor for singleton
    Logger();

    // Deleted copy constructor and assignment operator
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // Destructor
    ~Logger();

    // Convert log level to string
    std::string LogLevelToString(LogLevel level);

    // Get current timestamp as string
    std::string GetTimestamp();

    // Log file
    std::ofstream m_logFile;

    // Log levels
    LogLevel m_consoleLevel;
    LogLevel m_fileLevel;

    // Mutex for thread safety
    std::mutex m_mutex;

    // Initialization flag
    bool m_initialized;
};

// Convenience macros for logging
#define LOG_DEBUG(message) Logger::GetInstance().Debug(message)
#define LOG_INFO(message) Logger::GetInstance().Info(message)
#define LOG_WARNING(message) Logger::GetInstance().Warning(message)
#define LOG_ERROR(message) Logger::GetInstance().Error(message)
#define LOG_CRITICAL(message) Logger::GetInstance().Critical(message)

#endif // LOGGER_H
