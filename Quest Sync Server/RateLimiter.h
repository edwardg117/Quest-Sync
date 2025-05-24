#pragma once
#ifndef RATE_LIMITER_H
#define RATE_LIMITER_H

#include <string>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <vector>

/**
 * @brief Rate limit information for an IP address
 */
struct RateLimitInfo {
    std::vector<std::chrono::steady_clock::time_point> attempts;
    
    RateLimitInfo() = default;
};

/**
 * @brief Rate limiter class for controlling authentication attempts
 */
class RateLimiter {
public:
    /**
     * @brief Get the singleton instance of the rate limiter
     *
     * @return Reference to the rate limiter instance
     */
    static RateLimiter& GetInstance();

    /**
     * @brief Check if an IP address is allowed to make an authentication attempt
     *
     * @param ipAddress IP address to check
     * @param maxAttempts Maximum allowed attempts within the window
     * @param windowSeconds Time window in seconds
     * @return true if attempt is allowed, false if rate limited
     */
    bool IsAllowed(const std::string& ipAddress, int maxAttempts, int windowSeconds);

    /**
     * @brief Record an authentication attempt for an IP address
     *
     * @param ipAddress IP address making the attempt
     */
    void RecordAttempt(const std::string& ipAddress);

    /**
     * @brief Clean up old attempt records
     *
     * @param windowSeconds Time window in seconds (attempts older than this are removed)
     */
    void CleanupOldAttempts(int windowSeconds);

    /**
     * @brief Get the number of recent attempts for an IP address
     *
     * @param ipAddress IP address to check
     * @param windowSeconds Time window in seconds
     * @return Number of attempts within the window
     */
    int GetRecentAttempts(const std::string& ipAddress, int windowSeconds);

    /**
     * @brief Reset rate limit for an IP address (e.g., after successful authentication)
     *
     * @param ipAddress IP address to reset
     */
    void ResetRateLimit(const std::string& ipAddress);

    /**
     * @brief Get the number of IP addresses being tracked
     *
     * @return Number of tracked IP addresses
     */
    size_t GetTrackedIPCount();

private:
    RateLimiter() = default;
    ~RateLimiter() = default;
    RateLimiter(const RateLimiter&) = delete;
    RateLimiter& operator=(const RateLimiter&) = delete;

    /**
     * @brief Remove attempts older than the specified window
     *
     * @param info Rate limit info to clean
     * @param windowSeconds Time window in seconds
     */
    void CleanupAttemptsForIP(RateLimitInfo& info, int windowSeconds);

    std::unordered_map<std::string, RateLimitInfo> m_ipAttempts;
    std::mutex m_attemptsMutex;
};

#endif // RATE_LIMITER_H
