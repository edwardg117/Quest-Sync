#pragma once
#ifndef SESSION_MANAGER_H
#define SESSION_MANAGER_H

#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <random>

/**
 * @brief Session information structure
 */
struct SessionInfo {
    std::string token;
    std::chrono::steady_clock::time_point expiryTime;
    std::string clientIP;

    SessionInfo() = default;
    SessionInfo(const std::string& t, std::chrono::steady_clock::time_point expiry, const std::string& ip)
        : token(t), expiryTime(expiry), clientIP(ip) {}
};

/**
 * @brief Session manager class for handling session tokens
 */
class SessionManager {
public:
    /**
     * @brief Get the singleton instance of the session manager
     *
     * @return Reference to the session manager instance
     */
    static SessionManager& GetInstance();

    /**
     * @brief Generate a new session token for a client
     *
     * @param clientSocket Socket identifier for the client
     * @param clientIP IP address of the client
     * @param expirySeconds Token expiry time in seconds
     * @return Generated session token
     */
    std::string GenerateSessionToken(int clientSocket, const std::string& clientIP, int expirySeconds);

    /**
     * @brief Validate a session token
     *
     * @param clientSocket Socket identifier for the client
     * @param token Session token to validate
     * @return true if token is valid and not expired, false otherwise
     */
    bool ValidateSessionToken(int clientSocket, const std::string& token);

    /**
     * @brief Remove a session token (logout)
     *
     * @param clientSocket Socket identifier for the client
     */
    void RemoveSession(int clientSocket);

    /**
     * @brief Clean up expired sessions
     */
    void CleanupExpiredSessions();

    /**
     * @brief Get the number of active sessions
     *
     * @return Number of active sessions
     */
    size_t GetActiveSessionCount();

    /**
     * @brief Get the IP address for a client session
     *
     * @param clientSocket Socket identifier for the client
     * @return IP address of the client, or empty string if not found
     */
    std::string GetClientIP(int clientSocket);

private:
    SessionManager() = default;
    ~SessionManager() = default;
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;

    /**
     * @brief Generate a random token string
     *
     * @param length Length of the token
     * @return Random token string
     */
    std::string GenerateRandomToken(size_t length = 32);

    std::unordered_map<int, SessionInfo> m_sessions;
    std::mutex m_sessionsMutex;
    std::random_device m_randomDevice;
    std::mt19937 m_randomGenerator{m_randomDevice()};
};

#endif // SESSION_MANAGER_H
