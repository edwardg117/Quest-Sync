#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <chrono>
#include <vector>

#include "SessionManager.h"
#include "MockClasses.h"

class SessionManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Get the singleton instance
        m_sessionManager = &SessionManager::GetInstance();

        // Clean up any existing sessions from previous tests
        CleanupAllSessions();
    }

    void TearDown() override {
        // Clean up sessions after each test
        CleanupAllSessions();
    }

    void CleanupAllSessions() {
        // Remove all test sessions
        for (int i = 1; i <= 100; ++i) {
            m_sessionManager->RemoveSession(i);
        }
    }

    SessionManager* m_sessionManager;
};

// Test singleton pattern
TEST_F(SessionManagerTest, SingletonPattern) {
    SessionManager& instance1 = SessionManager::GetInstance();
    SessionManager& instance2 = SessionManager::GetInstance();

    // Should be the same instance
    EXPECT_EQ(&instance1, &instance2);
    EXPECT_EQ(m_sessionManager, &instance1);
}

// Test session token generation
TEST_F(SessionManagerTest, GenerateSessionToken) {
    int clientSocket = 1;
    std::string clientIP = "192.168.1.100";
    int expirySeconds = 3600;

    std::string token = m_sessionManager->GenerateSessionToken(clientSocket, clientIP, expirySeconds);

    // Token should not be empty
    EXPECT_FALSE(token.empty());

    // Token should be 32 characters by default
    EXPECT_EQ(token.length(), 32);

    // Token should contain only alphanumeric characters
    for (char c : token) {
        EXPECT_TRUE(std::isalnum(c));
    }
}

// Test session token uniqueness
TEST_F(SessionManagerTest, TokenUniqueness) {
    std::vector<std::string> tokens;

    // Generate multiple tokens
    for (int i = 1; i <= 10; ++i) {
        std::string token = m_sessionManager->GenerateSessionToken(i, "192.168.1." + std::to_string(i), 3600);
        tokens.push_back(token);
    }

    // All tokens should be unique
    for (size_t i = 0; i < tokens.size(); ++i) {
        for (size_t j = i + 1; j < tokens.size(); ++j) {
            EXPECT_NE(tokens[i], tokens[j]);
        }
    }
}

// Test session token validation - valid token
TEST_F(SessionManagerTest, ValidateSessionToken_Valid) {
    int clientSocket = 1;
    std::string clientIP = "192.168.1.100";
    int expirySeconds = 3600;

    std::string token = m_sessionManager->GenerateSessionToken(clientSocket, clientIP, expirySeconds);

    // Validation should succeed
    EXPECT_TRUE(m_sessionManager->ValidateSessionToken(clientSocket, token));
}

// Test session token validation - invalid token
TEST_F(SessionManagerTest, ValidateSessionToken_InvalidToken) {
    int clientSocket = 1;
    std::string clientIP = "192.168.1.100";
    int expirySeconds = 3600;

    std::string token = m_sessionManager->GenerateSessionToken(clientSocket, clientIP, expirySeconds);

    // Validation with wrong token should fail
    EXPECT_FALSE(m_sessionManager->ValidateSessionToken(clientSocket, "wrong_token"));
}

// Test session token validation - non-existent session
TEST_F(SessionManagerTest, ValidateSessionToken_NonExistentSession) {
    // Validation for non-existent session should fail
    EXPECT_FALSE(m_sessionManager->ValidateSessionToken(999, "any_token"));
}

// Test session token validation - expired token
TEST_F(SessionManagerTest, ValidateSessionToken_ExpiredToken) {
    int clientSocket = 1;
    std::string clientIP = "192.168.1.100";
    int expirySeconds = 1; // Very short expiry

    std::string token = m_sessionManager->GenerateSessionToken(clientSocket, clientIP, expirySeconds);

    // Wait for token to expire
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Validation should fail for expired token
    EXPECT_FALSE(m_sessionManager->ValidateSessionToken(clientSocket, token));

    // Session should be automatically removed
    EXPECT_EQ(m_sessionManager->GetActiveSessionCount(), 0);
}

// Test session removal
TEST_F(SessionManagerTest, RemoveSession) {
    int clientSocket = 1;
    std::string clientIP = "192.168.1.100";
    int expirySeconds = 3600;

    std::string token = m_sessionManager->GenerateSessionToken(clientSocket, clientIP, expirySeconds);

    // Session should exist
    EXPECT_TRUE(m_sessionManager->ValidateSessionToken(clientSocket, token));
    EXPECT_EQ(m_sessionManager->GetActiveSessionCount(), 1);

    // Remove session
    m_sessionManager->RemoveSession(clientSocket);

    // Session should no longer exist
    EXPECT_FALSE(m_sessionManager->ValidateSessionToken(clientSocket, token));
    EXPECT_EQ(m_sessionManager->GetActiveSessionCount(), 0);
}

// Test removing non-existent session
TEST_F(SessionManagerTest, RemoveNonExistentSession) {
    // Should not crash or cause issues
    m_sessionManager->RemoveSession(999);
    EXPECT_EQ(m_sessionManager->GetActiveSessionCount(), 0);
}

// Test cleanup expired sessions
TEST_F(SessionManagerTest, CleanupExpiredSessions) {
    // Create sessions with different expiry times
    m_sessionManager->GenerateSessionToken(1, "192.168.1.1", 1);  // Expires in 1 second
    m_sessionManager->GenerateSessionToken(2, "192.168.1.2", 3600); // Expires in 1 hour
    m_sessionManager->GenerateSessionToken(3, "192.168.1.3", 1);  // Expires in 1 second

    EXPECT_EQ(m_sessionManager->GetActiveSessionCount(), 3);

    // Wait for some sessions to expire
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Cleanup expired sessions
    m_sessionManager->CleanupExpiredSessions();

    // Only the long-lived session should remain
    EXPECT_EQ(m_sessionManager->GetActiveSessionCount(), 1);
}

// Test active session count
TEST_F(SessionManagerTest, GetActiveSessionCount) {
    EXPECT_EQ(m_sessionManager->GetActiveSessionCount(), 0);

    // Add sessions
    m_sessionManager->GenerateSessionToken(1, "192.168.1.1", 3600);
    EXPECT_EQ(m_sessionManager->GetActiveSessionCount(), 1);

    m_sessionManager->GenerateSessionToken(2, "192.168.1.2", 3600);
    EXPECT_EQ(m_sessionManager->GetActiveSessionCount(), 2);

    // Remove a session
    m_sessionManager->RemoveSession(1);
    EXPECT_EQ(m_sessionManager->GetActiveSessionCount(), 1);
}

// Test get client IP
TEST_F(SessionManagerTest, GetClientIP) {
    int clientSocket = 1;
    std::string clientIP = "192.168.1.100";

    m_sessionManager->GenerateSessionToken(clientSocket, clientIP, 3600);

    // Should return the correct IP
    EXPECT_EQ(m_sessionManager->GetClientIP(clientSocket), clientIP);

    // Should return empty string for non-existent session
    EXPECT_EQ(m_sessionManager->GetClientIP(999), "");
}

// Test session replacement
TEST_F(SessionManagerTest, SessionReplacement) {
    int clientSocket = 1;
    std::string clientIP = "192.168.1.100";

    // Generate first token
    std::string token1 = m_sessionManager->GenerateSessionToken(clientSocket, clientIP, 3600);
    EXPECT_TRUE(m_sessionManager->ValidateSessionToken(clientSocket, token1));

    // Generate second token for same client (should replace first)
    std::string token2 = m_sessionManager->GenerateSessionToken(clientSocket, clientIP, 3600);
    EXPECT_NE(token1, token2);

    // First token should no longer be valid
    EXPECT_FALSE(m_sessionManager->ValidateSessionToken(clientSocket, token1));

    // Second token should be valid
    EXPECT_TRUE(m_sessionManager->ValidateSessionToken(clientSocket, token2));

    // Should still have only one session
    EXPECT_EQ(m_sessionManager->GetActiveSessionCount(), 1);
}

// Test thread safety
TEST_F(SessionManagerTest, ThreadSafety) {
    const int numThreads = 10;
    const int operationsPerThread = 50;
    std::vector<std::thread> threads;

    // Create multiple threads that generate, validate, and remove sessions
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, i, operationsPerThread]() {
            for (int j = 0; j < operationsPerThread; ++j) {
                int clientSocket = i * operationsPerThread + j + 1;
                std::string clientIP = "192.168." + std::to_string(i) + "." + std::to_string(j);

                // Generate session
                std::string token = m_sessionManager->GenerateSessionToken(clientSocket, clientIP, 3600);

                // Validate session
                EXPECT_TRUE(m_sessionManager->ValidateSessionToken(clientSocket, token));

                // Get client IP
                EXPECT_EQ(m_sessionManager->GetClientIP(clientSocket), clientIP);

                // Remove session
                m_sessionManager->RemoveSession(clientSocket);
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // All sessions should be removed
    EXPECT_EQ(m_sessionManager->GetActiveSessionCount(), 0);
}

// Test concurrent session validation
TEST_F(SessionManagerTest, ConcurrentValidation) {
    const int numSessions = 20;
    std::vector<std::pair<int, std::string>> sessions;

    // Generate sessions
    for (int i = 1; i <= numSessions; ++i) {
        std::string token = m_sessionManager->GenerateSessionToken(i, "192.168.1." + std::to_string(i), 3600);
        sessions.emplace_back(i, token);
    }

    EXPECT_EQ(m_sessionManager->GetActiveSessionCount(), numSessions);

    // Validate sessions concurrently
    std::vector<std::thread> threads;
    std::atomic<int> validationCount{0};

    for (const auto& session : sessions) {
        threads.emplace_back([this, session, &validationCount]() {
            if (m_sessionManager->ValidateSessionToken(session.first, session.second)) {
                validationCount++;
            }
        });
    }

    // Wait for all validations to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // All validations should succeed
    EXPECT_EQ(validationCount.load(), numSessions);
}

// Test edge cases with zero expiry
TEST_F(SessionManagerTest, ZeroExpirySession) {
    int clientSocket = 1;
    std::string clientIP = "192.168.1.100";

    // Generate session with zero expiry (should expire immediately)
    std::string token = m_sessionManager->GenerateSessionToken(clientSocket, clientIP, 0);

    // Small delay to ensure expiry
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // Validation should fail
    EXPECT_FALSE(m_sessionManager->ValidateSessionToken(clientSocket, token));
}

// Test large number of sessions
TEST_F(SessionManagerTest, LargeNumberOfSessions) {
    const int numSessions = 1000;

    // Generate many sessions
    for (int i = 1; i <= numSessions; ++i) {
        m_sessionManager->GenerateSessionToken(i, "192.168." + std::to_string(i / 256) + "." + std::to_string(i % 256), 3600);
    }

    EXPECT_EQ(m_sessionManager->GetActiveSessionCount(), numSessions);

    // Validate random sessions
    for (int i = 0; i < 100; ++i) {
        int randomSocket = (rand() % numSessions) + 1;
        std::string expectedIP = "192.168." + std::to_string(randomSocket / 256) + "." + std::to_string(randomSocket % 256);
        EXPECT_EQ(m_sessionManager->GetClientIP(randomSocket), expectedIP);
    }
}

// Test cleanup with mixed expiry times
TEST_F(SessionManagerTest, MixedExpiryCleanup) {
    // Create sessions with various expiry times
    std::vector<int> expiredSockets;
    std::vector<int> validSockets;

    for (int i = 1; i <= 10; ++i) {
        if (i % 2 == 0) {
            // Even sockets: short expiry
            m_sessionManager->GenerateSessionToken(i, "192.168.1." + std::to_string(i), 1);
            expiredSockets.push_back(i);
        } else {
            // Odd sockets: long expiry
            m_sessionManager->GenerateSessionToken(i, "192.168.1." + std::to_string(i), 3600);
            validSockets.push_back(i);
        }
    }

    EXPECT_EQ(m_sessionManager->GetActiveSessionCount(), 10);

    // Wait for short-lived sessions to expire
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Cleanup expired sessions
    m_sessionManager->CleanupExpiredSessions();

    // Only long-lived sessions should remain
    EXPECT_EQ(m_sessionManager->GetActiveSessionCount(), validSockets.size());

    // Verify which sessions remain
    for (int socket : validSockets) {
        EXPECT_NE(m_sessionManager->GetClientIP(socket), "");
    }

    for (int socket : expiredSockets) {
        EXPECT_EQ(m_sessionManager->GetClientIP(socket), "");
    }
}
