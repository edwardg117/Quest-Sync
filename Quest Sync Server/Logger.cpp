#include "Logger.h"

// Get the singleton instance
Logger& Logger::GetInstance() {
    static Logger instance;
    return instance;
}

// Constructor
Logger::Logger() : m_consoleLevel(LogLevel::INFO), m_fileLevel(LogLevel::DEBUG), m_initialized(false) {
}

// Destructor
Logger::~Logger() {
    Close();
}

// Initialize the logger
bool Logger::Initialize(const std::string& logFilePath, LogLevel consoleLevel, LogLevel fileLevel) {
    try {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Set log levels
        m_consoleLevel = consoleLevel;
        m_fileLevel = fileLevel;

        // Try to create the directory if it doesn't exist
        size_t lastSlash = logFilePath.find_last_of("/\\");
        if (lastSlash != std::string::npos) {
            std::string directory = logFilePath.substr(0, lastSlash);
            // In a real application, we would create the directory here if needed
            // For simplicity, we'll just assume the directory exists
        }

        // Open log file
        try {
            m_logFile.open(logFilePath, std::ios::out | std::ios::app);
            if (!m_logFile.is_open()) {
                std::cerr << "Failed to open log file: " << logFilePath << std::endl;
                // Continue without file logging
            } else {
                m_initialized = true;
            }
        } catch (const std::exception& e) {
            std::cerr << "Exception opening log file: " << e.what() << std::endl;
            // Continue without file logging
        }

        // Log initialization to console only
        std::string timestamp = GetTimestamp();
        std::string levelStr = LogLevelToString(LogLevel::INFO);
        std::string formattedMessage = timestamp + " [" + levelStr + "] " + "Logger initialized";
        std::cout << formattedMessage << std::endl;

        // If file is open, log to file too
        if (m_initialized) {
            m_logFile << formattedMessage << std::endl;
            m_logFile.flush();
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception in Logger::Initialize: " << e.what() << std::endl;
        return false;
    }
}

// Log a message
void Logger::Log(LogLevel level, const std::string& message) {
    try {
        std::lock_guard<std::mutex> lock(m_mutex);

        // Format the log message
        std::string timestamp = GetTimestamp();
        std::string levelStr = LogLevelToString(level);
        std::string formattedMessage = timestamp + " [" + levelStr + "] " + message;

        // Log to console if level is high enough
        if (level >= m_consoleLevel) {
            // Set console color based on log level
            switch (level) {
                case LogLevel::DEBUG:
                    std::cout << formattedMessage << std::endl;
                    break;
                case LogLevel::INFO:
                    std::cout << formattedMessage << std::endl;
                    break;
                case LogLevel::WARNING:
                    std::cerr << formattedMessage << std::endl;
                    break;
                case LogLevel::ERR:
                    std::cerr << formattedMessage << std::endl;
                    break;
                case LogLevel::CRITICAL:
                    std::cerr << formattedMessage << std::endl;
                    break;
            }
        }

        // Log to file if initialized and level is high enough
        if (m_initialized && level >= m_fileLevel) {
            try {
                m_logFile << formattedMessage << std::endl;
                m_logFile.flush();

                // Check if the file is still good
                if (!m_logFile.good()) {
                    std::cerr << "Error writing to log file" << std::endl;
                    m_initialized = false;  // Disable file logging
                }
            } catch (const std::exception& e) {
                std::cerr << "Exception writing to log file: " << e.what() << std::endl;
                m_initialized = false;  // Disable file logging
            }
        }
    } catch (const std::exception& e) {
        // Last resort - print to stderr
        std::cerr << "Exception in Logger::Log: " << e.what() << std::endl;
        std::cerr << "Original message: " << message << std::endl;
    }
}

// Log a debug message
void Logger::Debug(const std::string& message) {
    Log(LogLevel::DEBUG, message);
}

// Log an info message
void Logger::Info(const std::string& message) {
    Log(LogLevel::INFO, message);
}

// Log a warning message
void Logger::Warning(const std::string& message) {
    Log(LogLevel::WARNING, message);
}

// Log an error message
void Logger::Error(const std::string& message) {
    Log(LogLevel::ERR, message);
}

// Log a critical message
void Logger::Critical(const std::string& message) {
    Log(LogLevel::CRITICAL, message);
}

// Set the console log level
void Logger::SetConsoleLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_consoleLevel = level;
}

// Set the file log level
void Logger::SetFileLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_fileLevel = level;
}

// Close the logger
void Logger::Close() {
    try {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_initialized) {
            try {
                // Log directly to console
                std::string timestamp = GetTimestamp();
                std::string levelStr = LogLevelToString(LogLevel::INFO);
                std::string formattedMessage = timestamp + " [" + levelStr + "] " + "Logger closed";
                std::cout << formattedMessage << std::endl;

                // Try to log to file
                if (m_logFile.is_open() && m_logFile.good()) {
                    m_logFile << formattedMessage << std::endl;
                    m_logFile.flush();
                    m_logFile.close();
                }
            } catch (const std::exception& e) {
                std::cerr << "Exception closing log file: " << e.what() << std::endl;
            }

            m_initialized = false;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception in Logger::Close: " << e.what() << std::endl;
    }
}

// Convert log level to string
std::string Logger::LogLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:
            return "DEBUG";
        case LogLevel::INFO:
            return "INFO";
        case LogLevel::WARNING:
            return "WARNING";
        case LogLevel::ERR:
            return "ERROR";
        case LogLevel::CRITICAL:
            return "CRITICAL";
        default:
            return "UNKNOWN";
    }
}

// Get current timestamp as string
std::string Logger::GetTimestamp() {
    try {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::stringstream ss;

        // Use localtime_s for safety
        struct tm timeinfo;
        try {
#ifdef _WIN32
            if (localtime_s(&timeinfo, &time) != 0) {
                // If localtime_s fails, use a default timestamp
                return "YYYY-MM-DD HH:MM:SS.000";
            }
#else
            if (localtime_r(&time, &timeinfo) == nullptr) {
                // If localtime_r fails, use a default timestamp
                return "YYYY-MM-DD HH:MM:SS.000";
            }
#endif

            ss << std::put_time(&timeinfo, "%Y-%m-%d %H:%M:%S");
            ss << '.' << std::setfill('0') << std::setw(3) << ms.count();

            return ss.str();
        } catch (const std::exception&) {
            // If formatting fails, return a default timestamp
            return "YYYY-MM-DD HH:MM:SS.000";
        }
    } catch (const std::exception&) {
        // If anything else fails, return a default timestamp
        return "YYYY-MM-DD HH:MM:SS.000";
    }
}
