#include "SessionManager.h"
#include "Logger.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

// Get the singleton instance
SessionManager& SessionManager::GetInstance() {
    static SessionManager instance;
    return instance;
}

// Generate a new session token
std::string SessionManager::GenerateSessionToken(int clientSocket, const std::string& clientIP, int expirySeconds) {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);

    // Generate a new token
    std::string token = GenerateRandomToken();

    // Calculate expiry time
    auto expiryTime = std::chrono::steady_clock::now() + std::chrono::seconds(expirySeconds);

    // Store the session
    m_sessions[clientSocket] = SessionInfo(token, expiryTime, clientIP);

    LOG_DEBUG("Generated session token for client " + std::to_string(clientSocket) +
              " (IP: " + clientIP + "), expires in " + std::to_string(expirySeconds) + " seconds");

    return token;
}

// Validate a session token
bool SessionManager::ValidateSessionToken(int clientSocket, const std::string& token) {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);

    auto it = m_sessions.find(clientSocket);
    if (it == m_sessions.end()) {
        LOG_DEBUG("Session validation failed: No session found for client " + std::to_string(clientSocket));
        return false;
    }

    const SessionInfo& session = it->second;

    // Check if token matches
    if (session.token != token) {
        LOG_DEBUG("Session validation failed: Token mismatch for client " + std::to_string(clientSocket));
        return false;
    }

    // Check if token is expired
    auto now = std::chrono::steady_clock::now();
    if (now > session.expiryTime) {
        LOG_DEBUG("Session validation failed: Token expired for client " + std::to_string(clientSocket));
        // Remove expired session
        m_sessions.erase(it);
        return false;
    }

    LOG_DEBUG("Session validation successful for client " + std::to_string(clientSocket));
    return true;
}

// Remove a session
void SessionManager::RemoveSession(int clientSocket) {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);

    auto it = m_sessions.find(clientSocket);
    if (it != m_sessions.end()) {
        LOG_DEBUG("Removing session for client " + std::to_string(clientSocket));
        m_sessions.erase(it);
    }
}

// Clean up expired sessions
void SessionManager::CleanupExpiredSessions() {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);

    auto now = std::chrono::steady_clock::now();
    size_t removedCount = 0;

    auto it = m_sessions.begin();
    while (it != m_sessions.end()) {
        if (now > it->second.expiryTime) {
            LOG_DEBUG("Cleaning up expired session for client " + std::to_string(it->first));
            it = m_sessions.erase(it);
            removedCount++;
        } else {
            ++it;
        }
    }

    if (removedCount > 0) {
        LOG_DEBUG("Cleaned up " + std::to_string(removedCount) + " expired sessions");
    }
}

// Get the number of active sessions
size_t SessionManager::GetActiveSessionCount() {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);
    return m_sessions.size();
}

// Get the IP address for a client session
std::string SessionManager::GetClientIP(int clientSocket) {
    std::lock_guard<std::mutex> lock(m_sessionsMutex);

    auto it = m_sessions.find(clientSocket);
    if (it != m_sessions.end()) {
        return it->second.clientIP;
    }

    return "";
}

// Generate a random token string
std::string SessionManager::GenerateRandomToken(size_t length) {
    const std::string chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::uniform_int_distribution<> dist(0, chars.size() - 1);

    std::string token;
    token.reserve(length);

    for (size_t i = 0; i < length; ++i) {
        token += chars[dist(m_randomGenerator)];
    }

    return token;
}
