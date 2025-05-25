#include "RateLimiter.h"
#include "Logger.h"
#include <algorithm>

// Get the singleton instance
RateLimiter& RateLimiter::GetInstance() {
    static RateLimiter instance;
    return instance;
}

// Check if an IP address is allowed to make an authentication attempt
bool RateLimiter::IsAllowed(const std::string& ipAddress, int maxAttempts, int windowSeconds) {
    std::lock_guard<std::mutex> lock(m_attemptsMutex);

    // If maxAttempts is 0, always deny
    if (maxAttempts <= 0) {
        return false;
    }

    auto it = m_ipAttempts.find(ipAddress);
    if (it == m_ipAttempts.end()) {
        // No previous attempts, allow
        return true;
    }

    RateLimitInfo& info = it->second;

    // Clean up old attempts
    CleanupAttemptsForIP(info, windowSeconds);

    // Check if under the limit
    bool allowed = static_cast<int>(info.attempts.size()) < maxAttempts;

    if (!allowed) {
        LOG_WARNING("Rate limit exceeded for IP " + ipAddress +
                   ": " + std::to_string(info.attempts.size()) + "/" + std::to_string(maxAttempts) +
                   " attempts in " + std::to_string(windowSeconds) + " seconds");
    }

    return allowed;
}

// Record an authentication attempt
void RateLimiter::RecordAttempt(const std::string& ipAddress) {
    std::lock_guard<std::mutex> lock(m_attemptsMutex);

    auto now = std::chrono::steady_clock::now();
    m_ipAttempts[ipAddress].attempts.push_back(now);

    LOG_DEBUG("Recorded authentication attempt for IP " + ipAddress +
              " (total attempts: " + std::to_string(m_ipAttempts[ipAddress].attempts.size()) + ")");
}

// Clean up old attempt records
void RateLimiter::CleanupOldAttempts(int windowSeconds) {
    std::lock_guard<std::mutex> lock(m_attemptsMutex);

    size_t totalCleaned = 0;
    auto it = m_ipAttempts.begin();

    while (it != m_ipAttempts.end()) {
        size_t beforeSize = it->second.attempts.size();
        CleanupAttemptsForIP(it->second, windowSeconds);
        size_t afterSize = it->second.attempts.size();

        totalCleaned += (beforeSize - afterSize);

        // Remove IP entries with no recent attempts
        if (it->second.attempts.empty()) {
            it = m_ipAttempts.erase(it);
        } else {
            ++it;
        }
    }

    if (totalCleaned > 0) {
        LOG_DEBUG("Cleaned up " + std::to_string(totalCleaned) + " old rate limit attempts");
    }
}

// Get the number of recent attempts for an IP address
int RateLimiter::GetRecentAttempts(const std::string& ipAddress, int windowSeconds) {
    std::lock_guard<std::mutex> lock(m_attemptsMutex);

    auto it = m_ipAttempts.find(ipAddress);
    if (it == m_ipAttempts.end()) {
        return 0;
    }

    RateLimitInfo& info = it->second;
    CleanupAttemptsForIP(info, windowSeconds);

    return static_cast<int>(info.attempts.size());
}

// Reset rate limit for an IP address
void RateLimiter::ResetRateLimit(const std::string& ipAddress) {
    std::lock_guard<std::mutex> lock(m_attemptsMutex);

    auto it = m_ipAttempts.find(ipAddress);
    if (it != m_ipAttempts.end()) {
        LOG_DEBUG("Resetting rate limit for IP " + ipAddress);
        m_ipAttempts.erase(it);
    }
}

// Get the number of IP addresses being tracked
size_t RateLimiter::GetTrackedIPCount() {
    std::lock_guard<std::mutex> lock(m_attemptsMutex);
    return m_ipAttempts.size();
}

// Remove attempts older than the specified window
void RateLimiter::CleanupAttemptsForIP(RateLimitInfo& info, int windowSeconds) {
    auto now = std::chrono::steady_clock::now();
    auto cutoffTime = now - std::chrono::seconds(windowSeconds);

    // Remove attempts older than the window
    info.attempts.erase(
        std::remove_if(info.attempts.begin(), info.attempts.end(),
                      [cutoffTime](const std::chrono::steady_clock::time_point& attempt) {
                          return attempt < cutoffTime;
                      }),
        info.attempts.end()
    );
}
