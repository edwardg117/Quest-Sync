#pragma once
#ifndef QUESTSYNC_LOGGING_H
#define QUESTSYNC_LOGGING_H

// Undefine Windows ERROR macro to avoid conflicts
#ifdef ERROR
#undef ERROR
#endif

// Log level enumeration for Quest Sync
enum class QuestSyncLogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3  // Now safe to use ERROR since we undefined the macro
};

// Centralized logging function for Quest Sync
void QuestSyncLog(QuestSyncLogLevel level, const char* fmt, ...);

// Function to set log level from config
void SetLogLevelFromConfig();

// Function to get current log level
QuestSyncLogLevel GetCurrentLogLevel();

// Convenience macros for different log levels
#define QUESTSYNC_LOG_DEBUG(fmt, ...) QuestSyncLog(QuestSyncLogLevel::DEBUG, fmt, ##__VA_ARGS__)
#define QUESTSYNC_LOG_INFO(fmt, ...) QuestSyncLog(QuestSyncLogLevel::INFO, fmt, ##__VA_ARGS__)
#define QUESTSYNC_LOG_WARNING(fmt, ...) QuestSyncLog(QuestSyncLogLevel::WARNING, fmt, ##__VA_ARGS__)
#define QUESTSYNC_LOG_ERROR(fmt, ...) QuestSyncLog(QuestSyncLogLevel::ERROR, fmt, ##__VA_ARGS__)

#endif // QUESTSYNC_LOGGING_H
