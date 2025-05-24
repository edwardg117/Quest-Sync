#include "QuestSyncLogging.h"
#include "Config.h"
#include "nvse/PluginAPI.h"
#include <cstdarg>
#include <cstdio>

// External reference to the global log from main.cpp
extern IDebugLog gLog;

// Global log level setting
static QuestSyncLogLevel g_logLevel = QuestSyncLogLevel::INFO;

// Centralized logging function for Quest Sync
void QuestSyncLog(QuestSyncLogLevel level, const char* fmt, ...) {
    // Only log if the message level is at or above the current log level
    if (level < g_logLevel) {
        return;
    }

    va_list args;
    va_start(args, fmt);

    // Format the message with a prefix indicating the log level
    char buffer[2048];
    const char* levelPrefix = "";
    switch (level) {
        case QuestSyncLogLevel::DEBUG:   levelPrefix = "[DEBUG] "; break;
        case QuestSyncLogLevel::INFO:    levelPrefix = "[INFO] "; break;
        case QuestSyncLogLevel::WARNING: levelPrefix = "[WARNING] "; break;
        case QuestSyncLogLevel::ERROR:   levelPrefix = "[ERROR] "; break;
    }

    // Create the full message with level prefix
    snprintf(buffer, sizeof(buffer), "%s%s", levelPrefix, fmt);

    // Use NVSE's logging system
    gLog.Log(IDebugLog::kLevel_Message, buffer, args);

    va_end(args);
}

// Function to set log level from config
void SetLogLevelFromConfig() {
    Config& config = Config::GetInstance();
    std::string logLevelStr = config.GetString("Client.LogLevel", "INFO");

    if (logLevelStr == "DEBUG") {
        g_logLevel = QuestSyncLogLevel::DEBUG;
    } else if (logLevelStr == "INFO") {
        g_logLevel = QuestSyncLogLevel::INFO;
    } else if (logLevelStr == "WARNING") {
        g_logLevel = QuestSyncLogLevel::WARNING;
    } else if (logLevelStr == "ERROR") {
        g_logLevel = QuestSyncLogLevel::ERROR;
    } else {
        // Default to INFO if invalid value
        g_logLevel = QuestSyncLogLevel::INFO;
        QUESTSYNC_LOG_WARNING("Invalid log level '%s' in config, defaulting to INFO", logLevelStr.c_str());
    }

    QUESTSYNC_LOG_INFO("Log level set to: %s", logLevelStr.c_str());
}

// Function to get current log level
QuestSyncLogLevel GetCurrentLogLevel() {
    return g_logLevel;
}
