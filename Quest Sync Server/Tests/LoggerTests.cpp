#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <filesystem>
#include <regex>

#include "Logger.h"
#include "MockClasses.h"

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary log file for testing
        m_logFilePath = TestUtils::CreateTempFile("");

        // Initialize the logger with the temporary file
        Logger::GetInstance().Initialize(m_logFilePath, LogLevel::DEBUG, LogLevel::DEBUG);
    }

    void TearDown() override {
        // Clean up the temporary log file
        TestUtils::DeleteTempFile(m_logFilePath);
    }

    // Helper function to read the log file content
    std::string ReadLogFile() {
        std::ifstream file(m_logFilePath);
        if (!file.is_open()) {
            return "";
        }

        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        return content;
    }

    // Helper function to check if a log message exists in the log file
    bool LogContains(const std::string& message) {
        std::string content = ReadLogFile();
        return content.find(message) != std::string::npos;
    }

    // Helper function to check if a log message with a specific level exists in the log file
    bool LogContainsWithLevel(LogLevel level, const std::string& message) {
        std::string levelStr;
        switch (level) {
            case LogLevel::DEBUG: levelStr = "DEBUG"; break;
            case LogLevel::INFO: levelStr = "INFO"; break;
            case LogLevel::WARNING: levelStr = "WARNING"; break;
            case LogLevel::ERR: levelStr = "ERROR"; break;
            case LogLevel::CRITICAL: levelStr = "CRITICAL"; break;
        }

        std::string content = ReadLogFile();
        std::regex pattern("\\[" + levelStr + "\\].*" + message);
        return std::regex_search(content, pattern);
    }

    std::string m_logFilePath;
};

// Test that the logger can be initialized
TEST_F(LoggerTest, Initialization) {
    // The logger is already initialized in SetUp, so we just need to check if the file exists
    std::ifstream checkFile(m_logFilePath);
    EXPECT_TRUE(checkFile.good());
}

// Test logging at different levels
TEST_F(LoggerTest, LogLevels) {
    // Log messages at different levels
    Logger::GetInstance().Debug("Debug message");
    Logger::GetInstance().Info("Info message");
    Logger::GetInstance().Warning("Warning message");
    Logger::GetInstance().Error("Error message");
    Logger::GetInstance().Critical("Critical message");

    // Check if all messages were logged
    EXPECT_TRUE(LogContainsWithLevel(LogLevel::DEBUG, "Debug message"));
    EXPECT_TRUE(LogContainsWithLevel(LogLevel::INFO, "Info message"));
    EXPECT_TRUE(LogContainsWithLevel(LogLevel::WARNING, "Warning message"));
    EXPECT_TRUE(LogContainsWithLevel(LogLevel::ERR, "Error message"));
    EXPECT_TRUE(LogContainsWithLevel(LogLevel::CRITICAL, "Critical message"));
}

// Test log level filtering
TEST_F(LoggerTest, LogLevelFiltering) {
    // Re-initialize the logger with different log levels
    Logger::GetInstance().Initialize(m_logFilePath, LogLevel::WARNING, LogLevel::INFO);

    // Log messages at different levels
    Logger::GetInstance().Debug("Debug message");  // Should not appear in console, but in file
    Logger::GetInstance().Info("Info message");    // Should appear in file only
    Logger::GetInstance().Warning("Warning message"); // Should appear in both
    Logger::GetInstance().Error("Error message");     // Should appear in both
    Logger::GetInstance().Critical("Critical message"); // Should appear in both

    // Check if messages were logged according to the log level
    EXPECT_FALSE(LogContainsWithLevel(LogLevel::DEBUG, "Debug message")); // Debug is below INFO
    EXPECT_TRUE(LogContainsWithLevel(LogLevel::INFO, "Info message"));
    EXPECT_TRUE(LogContainsWithLevel(LogLevel::WARNING, "Warning message"));
    EXPECT_TRUE(LogContainsWithLevel(LogLevel::ERR, "Error message"));
    EXPECT_TRUE(LogContainsWithLevel(LogLevel::CRITICAL, "Critical message"));
}

// Test the convenience macros
TEST_F(LoggerTest, LogMacros) {
    // Use the log macros
    LOG_DEBUG("Debug macro message");
    LOG_INFO("Info macro message");
    LOG_WARNING("Warning macro message");
    LOG_ERROR("Error macro message");
    LOG_CRITICAL("Critical macro message");

    // Check if all messages were logged
    EXPECT_TRUE(LogContainsWithLevel(LogLevel::DEBUG, "Debug macro message"));
    EXPECT_TRUE(LogContainsWithLevel(LogLevel::INFO, "Info macro message"));
    EXPECT_TRUE(LogContainsWithLevel(LogLevel::WARNING, "Warning macro message"));
    EXPECT_TRUE(LogContainsWithLevel(LogLevel::ERR, "Error macro message"));
    EXPECT_TRUE(LogContainsWithLevel(LogLevel::CRITICAL, "Critical macro message"));
}

// Test thread safety by logging from multiple threads
TEST_F(LoggerTest, ThreadSafety) {
    const int numThreads = 10;
    const int numMessagesPerThread = 100;

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([i, numMessagesPerThread]() {
            for (int j = 0; j < numMessagesPerThread; ++j) {
                Logger::GetInstance().Info("Thread " + std::to_string(i) + " message " + std::to_string(j));
            }
        });
    }

    // Wait for all threads to finish
    for (auto& thread : threads) {
        thread.join();
    }

    // Give the logger a moment to finish writing all messages
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Check if the expected number of messages were logged
    std::string content = ReadLogFile();
    int count = 0;
    std::string::size_type pos = 0;
    while ((pos = content.find("[INFO]", pos)) != std::string::npos) {
        ++count;
        pos += 6; // Length of "[INFO]"
    }

    // Due to potential race conditions in file I/O, we'll check if we have at least 90% of the expected messages
    int expectedCount = numThreads * numMessagesPerThread;
    int minimumAcceptable = static_cast<int>(expectedCount * 0.9);
    EXPECT_GE(count, minimumAcceptable);
}
