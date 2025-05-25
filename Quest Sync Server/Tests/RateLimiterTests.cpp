#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>

#include "RateLimiter.h"
#include "MockClasses.h"

class RateLimiterTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Get the singleton instance
        m_rateLimiter = &RateLimiter::GetInstance();

        // Clean up any existing rate limit data from previous tests
        CleanupAllRateLimits();
    }

    void TearDown() override {
        // Clean up rate limits after each test
        CleanupAllRateLimits();
    }

    void CleanupAllRateLimits() {
        // Reset rate limits for common test IPs
        for (int i = 1; i <= 255; ++i) {
            m_rateLimiter->ResetRateLimit("192.168.1." + std::to_string(i));
            m_rateLimiter->ResetRateLimit("10.0.0." + std::to_string(i));
            m_rateLimiter->ResetRateLimit("127.0.0." + std::to_string(i));
        }

        // Clean up old attempts with a very long window to remove everything
        m_rateLimiter->CleanupOldAttempts(86400); // 24 hours
    }

    RateLimiter* m_rateLimiter;
};

// Test singleton pattern
TEST_F(RateLimiterTest, SingletonPattern) {
    RateLimiter& instance1 = RateLimiter::GetInstance();
    RateLimiter& instance2 = RateLimiter::GetInstance();

    // Should be the same instance
    EXPECT_EQ(&instance1, &instance2);
    EXPECT_EQ(m_rateLimiter, &instance1);
}

// Test initial state - should allow requests
TEST_F(RateLimiterTest, InitialState) {
    std::string testIP = "192.168.1.100";

    // Should allow initial request
    EXPECT_TRUE(m_rateLimiter->IsAllowed(testIP, 5, 60));

    // Should have zero recent attempts initially
    EXPECT_EQ(m_rateLimiter->GetRecentAttempts(testIP, 60), 0);

    // Should have zero tracked IPs initially
    EXPECT_EQ(m_rateLimiter->GetTrackedIPCount(), 0);
}

// Test recording attempts
TEST_F(RateLimiterTest, RecordAttempt) {
    std::string testIP = "192.168.1.100";

    // Record an attempt
    m_rateLimiter->RecordAttempt(testIP);

    // Should have one recent attempt
    EXPECT_EQ(m_rateLimiter->GetRecentAttempts(testIP, 60), 1);

    // Should have one tracked IP
    EXPECT_EQ(m_rateLimiter->GetTrackedIPCount(), 1);

    // Record another attempt
    m_rateLimiter->RecordAttempt(testIP);

    // Should have two recent attempts
    EXPECT_EQ(m_rateLimiter->GetRecentAttempts(testIP, 60), 2);

    // Should still have one tracked IP
    EXPECT_EQ(m_rateLimiter->GetTrackedIPCount(), 1);
}

// Test rate limiting - under limit
TEST_F(RateLimiterTest, RateLimiting_UnderLimit) {
    std::string testIP = "192.168.1.100";
    int maxAttempts = 5;
    int windowSeconds = 60;

    // Make attempts under the limit
    for (int i = 0; i < maxAttempts - 1; ++i) {
        EXPECT_TRUE(m_rateLimiter->IsAllowed(testIP, maxAttempts, windowSeconds));
        m_rateLimiter->RecordAttempt(testIP);
    }

    // Should still be allowed
    EXPECT_TRUE(m_rateLimiter->IsAllowed(testIP, maxAttempts, windowSeconds));

    // Should have the expected number of attempts
    EXPECT_EQ(m_rateLimiter->GetRecentAttempts(testIP, windowSeconds), maxAttempts - 1);
}

// Test rate limiting - at limit
TEST_F(RateLimiterTest, RateLimiting_AtLimit) {
    std::string testIP = "192.168.1.100";
    int maxAttempts = 5;
    int windowSeconds = 60;

    // Make attempts up to the limit
    for (int i = 0; i < maxAttempts; ++i) {
        EXPECT_TRUE(m_rateLimiter->IsAllowed(testIP, maxAttempts, windowSeconds));
        m_rateLimiter->RecordAttempt(testIP);
    }

    // Should now be rate limited
    EXPECT_FALSE(m_rateLimiter->IsAllowed(testIP, maxAttempts, windowSeconds));

    // Should have the maximum number of attempts
    EXPECT_EQ(m_rateLimiter->GetRecentAttempts(testIP, windowSeconds), maxAttempts);
}

// Test rate limiting - over limit
TEST_F(RateLimiterTest, RateLimiting_OverLimit) {
    std::string testIP = "192.168.1.100";
    int maxAttempts = 3;
    int windowSeconds = 60;

    // Make attempts over the limit
    for (int i = 0; i < maxAttempts + 2; ++i) {
        if (i < maxAttempts) {
            EXPECT_TRUE(m_rateLimiter->IsAllowed(testIP, maxAttempts, windowSeconds));
        } else {
            EXPECT_FALSE(m_rateLimiter->IsAllowed(testIP, maxAttempts, windowSeconds));
        }
        m_rateLimiter->RecordAttempt(testIP);
    }

    // Should have more than the maximum number of attempts
    EXPECT_GT(m_rateLimiter->GetRecentAttempts(testIP, windowSeconds), maxAttempts);
}

// Test multiple IP addresses
TEST_F(RateLimiterTest, MultipleIPAddresses) {
    std::vector<std::string> testIPs = {
        "192.168.1.100",
        "192.168.1.101",
        "10.0.0.1",
        "127.0.0.1"
    };
    int maxAttempts = 3;
    int windowSeconds = 60;

    // Each IP should be tracked independently
    for (const auto& ip : testIPs) {
        // Make attempts up to the limit for each IP
        for (int i = 0; i < maxAttempts; ++i) {
            EXPECT_TRUE(m_rateLimiter->IsAllowed(ip, maxAttempts, windowSeconds));
            m_rateLimiter->RecordAttempt(ip);
        }

        // Each IP should now be rate limited
        EXPECT_FALSE(m_rateLimiter->IsAllowed(ip, maxAttempts, windowSeconds));
        EXPECT_EQ(m_rateLimiter->GetRecentAttempts(ip, windowSeconds), maxAttempts);
    }

    // Should have all IPs tracked
    EXPECT_EQ(m_rateLimiter->GetTrackedIPCount(), testIPs.size());
}

// Test time window expiry
TEST_F(RateLimiterTest, TimeWindowExpiry) {
    std::string testIP = "192.168.1.100";
    int maxAttempts = 3;
    int windowSeconds = 2; // Short window for testing

    // Make attempts up to the limit
    for (int i = 0; i < maxAttempts; ++i) {
        EXPECT_TRUE(m_rateLimiter->IsAllowed(testIP, maxAttempts, windowSeconds));
        m_rateLimiter->RecordAttempt(testIP);
    }

    // Should be rate limited
    EXPECT_FALSE(m_rateLimiter->IsAllowed(testIP, maxAttempts, windowSeconds));

    // Wait for window to expire
    std::this_thread::sleep_for(std::chrono::seconds(windowSeconds + 1));

    // Should be allowed again after window expires
    EXPECT_TRUE(m_rateLimiter->IsAllowed(testIP, maxAttempts, windowSeconds));

    // Should have zero recent attempts after expiry
    EXPECT_EQ(m_rateLimiter->GetRecentAttempts(testIP, windowSeconds), 0);
}

// Test reset rate limit
TEST_F(RateLimiterTest, ResetRateLimit) {
    std::string testIP = "192.168.1.100";
    int maxAttempts = 3;
    int windowSeconds = 60;

    // Make attempts up to the limit
    for (int i = 0; i < maxAttempts; ++i) {
        m_rateLimiter->RecordAttempt(testIP);
    }

    // Should be rate limited
    EXPECT_FALSE(m_rateLimiter->IsAllowed(testIP, maxAttempts, windowSeconds));
    EXPECT_EQ(m_rateLimiter->GetRecentAttempts(testIP, windowSeconds), maxAttempts);

    // Reset rate limit
    m_rateLimiter->ResetRateLimit(testIP);

    // Should be allowed again
    EXPECT_TRUE(m_rateLimiter->IsAllowed(testIP, maxAttempts, windowSeconds));
    EXPECT_EQ(m_rateLimiter->GetRecentAttempts(testIP, windowSeconds), 0);
}

// Test reset non-existent rate limit
TEST_F(RateLimiterTest, ResetNonExistentRateLimit) {
    std::string testIP = "192.168.1.100";

    // Should not crash or cause issues
    m_rateLimiter->ResetRateLimit(testIP);

    // Should still be allowed
    EXPECT_TRUE(m_rateLimiter->IsAllowed(testIP, 5, 60));
}

// Test cleanup old attempts
TEST_F(RateLimiterTest, CleanupOldAttempts) {
    std::vector<std::string> testIPs = {
        "192.168.1.100",
        "192.168.1.101",
        "192.168.1.102"
    };
    int windowSeconds = 2; // Short window for testing

    // Record attempts for multiple IPs
    for (const auto& ip : testIPs) {
        for (int i = 0; i < 3; ++i) {
            m_rateLimiter->RecordAttempt(ip);
        }
    }

    EXPECT_EQ(m_rateLimiter->GetTrackedIPCount(), testIPs.size());

    // Wait for attempts to become old
    std::this_thread::sleep_for(std::chrono::seconds(windowSeconds + 1));

    // Cleanup old attempts
    m_rateLimiter->CleanupOldAttempts(windowSeconds);

    // All IPs should be removed from tracking (no recent attempts)
    EXPECT_EQ(m_rateLimiter->GetTrackedIPCount(), 0);

    // All IPs should have zero recent attempts
    for (const auto& ip : testIPs) {
        EXPECT_EQ(m_rateLimiter->GetRecentAttempts(ip, windowSeconds), 0);
    }
}

// Test partial cleanup (some attempts expire, some don't)
TEST_F(RateLimiterTest, PartialCleanup) {
    std::string testIP = "192.168.1.100";
    int windowSeconds = 3;

    // Record some attempts
    m_rateLimiter->RecordAttempt(testIP);
    m_rateLimiter->RecordAttempt(testIP);

    // Wait for some time (but not full window)
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Record more attempts
    m_rateLimiter->RecordAttempt(testIP);
    m_rateLimiter->RecordAttempt(testIP);

    // Should have 4 attempts total
    EXPECT_EQ(m_rateLimiter->GetRecentAttempts(testIP, windowSeconds), 4);

    // Wait for first attempts to expire
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Should now have only 2 recent attempts (the later ones)
    EXPECT_EQ(m_rateLimiter->GetRecentAttempts(testIP, windowSeconds), 2);
}

// Test thread safety
TEST_F(RateLimiterTest, ThreadSafety) {
    const int numThreads = 10;
    const int attemptsPerThread = 20;
    std::vector<std::thread> threads;
    std::atomic<int> totalAttempts{0};

    std::string testIP = "192.168.1.100";
    int maxAttempts = 50;
    int windowSeconds = 60;

    // Create multiple threads that record attempts
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, testIP, attemptsPerThread, &totalAttempts]() {
            for (int j = 0; j < attemptsPerThread; ++j) {
                m_rateLimiter->RecordAttempt(testIP);
                totalAttempts++;
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Should have recorded all attempts
    int expectedAttempts = numThreads * attemptsPerThread;
    EXPECT_EQ(totalAttempts.load(), expectedAttempts);
    EXPECT_EQ(m_rateLimiter->GetRecentAttempts(testIP, windowSeconds), expectedAttempts);
}

// Test concurrent rate limiting checks
TEST_F(RateLimiterTest, ConcurrentRateLimitingChecks) {
    const int numThreads = 20;
    std::vector<std::thread> threads;
    std::atomic<int> allowedCount{0};
    std::atomic<int> deniedCount{0};

    std::string testIP = "192.168.1.100";
    int maxAttempts = 5;
    int windowSeconds = 60;

    // Pre-populate with some attempts to get close to the limit
    for (int i = 0; i < maxAttempts - 1; ++i) {
        m_rateLimiter->RecordAttempt(testIP);
    }

    // Create multiple threads that check rate limiting
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, testIP, maxAttempts, windowSeconds, &allowedCount, &deniedCount]() {
            if (m_rateLimiter->IsAllowed(testIP, maxAttempts, windowSeconds)) {
                allowedCount++;
            } else {
                deniedCount++;
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Should have some allowed and some denied (exact numbers depend on timing)
    EXPECT_GT(allowedCount.load() + deniedCount.load(), 0);
    EXPECT_EQ(allowedCount.load() + deniedCount.load(), numThreads);
}

// Test different window sizes
TEST_F(RateLimiterTest, DifferentWindowSizes) {
    std::string testIP = "192.168.1.100";

    // Record attempts
    for (int i = 0; i < 5; ++i) {
        m_rateLimiter->RecordAttempt(testIP);
        if (i < 4) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    // Different window sizes should show different attempt counts
    EXPECT_EQ(m_rateLimiter->GetRecentAttempts(testIP, 1), 1);  // Only last attempt
    EXPECT_EQ(m_rateLimiter->GetRecentAttempts(testIP, 3), 3);  // Last 3 attempts
    EXPECT_EQ(m_rateLimiter->GetRecentAttempts(testIP, 10), 5); // All attempts
}

// Test edge case: zero max attempts
TEST_F(RateLimiterTest, ZeroMaxAttempts) {
    std::string testIP = "192.168.1.100";

    // Should always be denied with zero max attempts
    EXPECT_FALSE(m_rateLimiter->IsAllowed(testIP, 0, 60));

    // Even after recording an attempt
    m_rateLimiter->RecordAttempt(testIP);
    EXPECT_FALSE(m_rateLimiter->IsAllowed(testIP, 0, 60));
}

// Test edge case: zero window
TEST_F(RateLimiterTest, ZeroWindow) {
    std::string testIP = "192.168.1.100";

    // Record an attempt
    m_rateLimiter->RecordAttempt(testIP);

    // With zero window, should have zero recent attempts
    EXPECT_EQ(m_rateLimiter->GetRecentAttempts(testIP, 0), 0);

    // Should be allowed with zero window (no recent attempts)
    EXPECT_TRUE(m_rateLimiter->IsAllowed(testIP, 5, 0));
}

// Test large number of IPs
TEST_F(RateLimiterTest, LargeNumberOfIPs) {
    const int numIPs = 1000;
    int maxAttempts = 3;
    int windowSeconds = 60;

    // Generate many IPs and record attempts
    for (int i = 1; i <= numIPs; ++i) {
        std::string ip = "10." + std::to_string(i / 256) + "." + std::to_string((i % 256) / 256) + "." + std::to_string(i % 256);

        for (int j = 0; j < maxAttempts; ++j) {
            m_rateLimiter->RecordAttempt(ip);
        }
    }

    EXPECT_EQ(m_rateLimiter->GetTrackedIPCount(), numIPs);

    // Verify random IPs are rate limited
    for (int i = 0; i < 100; ++i) {
        int randomIP = (rand() % numIPs) + 1;
        std::string ip = "10." + std::to_string(randomIP / 256) + "." + std::to_string((randomIP % 256) / 256) + "." + std::to_string(randomIP % 256);
        EXPECT_FALSE(m_rateLimiter->IsAllowed(ip, maxAttempts, windowSeconds));
    }
}

// Test cleanup with mixed timing
TEST_F(RateLimiterTest, MixedTimingCleanup) {
    std::vector<std::string> oldIPs = {"192.168.1.1", "192.168.1.2"};
    std::vector<std::string> newIPs = {"192.168.1.3", "192.168.1.4"};
    int windowSeconds = 3;

    // Record attempts for "old" IPs
    for (const auto& ip : oldIPs) {
        m_rateLimiter->RecordAttempt(ip);
    }

    // Wait for old attempts to age
    std::this_thread::sleep_for(std::chrono::seconds(windowSeconds + 1));

    // Record attempts for "new" IPs
    for (const auto& ip : newIPs) {
        m_rateLimiter->RecordAttempt(ip);
    }

    // Should have all IPs tracked
    EXPECT_EQ(m_rateLimiter->GetTrackedIPCount(), oldIPs.size() + newIPs.size());

    // Cleanup old attempts
    m_rateLimiter->CleanupOldAttempts(windowSeconds);

    // Should only have new IPs tracked
    EXPECT_EQ(m_rateLimiter->GetTrackedIPCount(), newIPs.size());

    // Verify old IPs have no recent attempts
    for (const auto& ip : oldIPs) {
        EXPECT_EQ(m_rateLimiter->GetRecentAttempts(ip, windowSeconds), 0);
    }

    // Verify new IPs still have recent attempts
    for (const auto& ip : newIPs) {
        EXPECT_EQ(m_rateLimiter->GetRecentAttempts(ip, windowSeconds), 1);
    }
}
