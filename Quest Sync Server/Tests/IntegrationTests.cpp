#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>
#include <WS2tcpip.h>

#include "TCPServer.h"
#include "SessionManager.h"
#include "RateLimiter.h"
#include "Config.h"
#include "Logger.h"
#include "Message.h"
#include "MockClasses.h"

// Integration test fixture for testing component interactions
class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize logger for tests
        Logger::GetInstance().Initialize("integration_test.log");
        Logger::GetInstance().SetConsoleLevel(LogLevel::ERR); // Reduce noise during tests
        Logger::GetInstance().SetFileLevel(LogLevel::DEBUG);

        // Get singleton instances
        m_config = &Config::GetInstance();
        m_sessionManager = &SessionManager::GetInstance();
        m_rateLimiter = &RateLimiter::GetInstance();

        // Set up test configuration
        SetupTestConfig();

        // Clean up any existing state
        CleanupTestState();

        // Initialize message handler
        m_messageHandler = [this](TCPServer* server, SOCKET client, const Message& message) {
            HandleTestMessage(server, client, message);
        };

        // Create server instance
        m_server = std::make_unique<TCPServer>("127.0.0.1", 25580, m_messageHandler);
    }

    void TearDown() override {
        // Stop server if running
        if (m_server && m_server->IsRunning()) {
            m_server->Stop();
        }

        // Clean up test state
        CleanupTestState();

        // Reset configuration to defaults
        ResetTestConfig();
    }

    void SetupTestConfig() {
        // Set test-specific configuration
        m_config->SetString("Server.IpAddress", "127.0.0.1");
        m_config->SetInt("Server.Port", 25580);
        m_config->SetBool("Security.EnableAuthentication", true);
        m_config->SetString("Security.Password", "test_password");
        m_config->SetInt("Security.SessionTokenExpiry", 3600);
        m_config->SetBool("Security.RateLimitEnabled", true);
        m_config->SetInt("Security.MaxAuthAttempts", 3);
        m_config->SetInt("Security.RateLimitWindow", 60);
        m_config->SetString("Logging.ConsoleLevel", "ERROR");
        m_config->SetString("Logging.FileLevel", "DEBUG");
    }

    void ResetTestConfig() {
        // Reset to default values
        m_config->SetString("Server.IpAddress", "");
        m_config->SetInt("Server.Port", 25575);
        m_config->SetBool("Security.EnableAuthentication", false);
        m_config->SetString("Security.Password", "");
        m_config->SetInt("Security.SessionTokenExpiry", 3600);
        m_config->SetBool("Security.RateLimitEnabled", true);
        m_config->SetInt("Security.MaxAuthAttempts", 5);
        m_config->SetInt("Security.RateLimitWindow", 300);
    }

    void CleanupTestState() {
        // Clean up sessions
        for (int i = 1; i <= 100; ++i) {
            m_sessionManager->RemoveSession(i);
        }

        // Clean up rate limits
        for (int i = 1; i <= 255; ++i) {
            m_rateLimiter->ResetRateLimit("127.0.0." + std::to_string(i));
            m_rateLimiter->ResetRateLimit("192.168.1." + std::to_string(i));
        }
        m_rateLimiter->CleanupOldAttempts(86400); // 24 hours

        // Reset message counters
        {
            std::lock_guard<std::mutex> lock(m_messagesMutex);
            m_receivedMessages.clear();
            m_messageCount = 0;
        }
    }

    void HandleTestMessage(TCPServer* server, SOCKET client, const Message& message) {
        std::lock_guard<std::mutex> lock(m_messagesMutex);
        m_receivedMessages.push_back({client, message.GetType(), message.GetPayloadAsString()});
        m_messageCount++;
    }

    // Helper function to simulate client authentication
    bool SimulateClientAuthentication(SOCKET clientSocket, const std::string& password, std::string& sessionToken) {
        // Create handshake request (this is what the server actually uses for authentication)
        HandshakeRequest handshakeRequest;
        handshakeRequest.clientVersion = Version::ServerVersion; // Use compatible version
        handshakeRequest.password = password;

        Message handshakeMsg(MessageType::HANDSHAKE_REQUEST);
        std::vector<uint8_t> payload = handshakeRequest.Serialize();
        handshakeMsg.SetPayload(payload);

        // Simulate server processing the handshake/authentication
        std::string clientIP = "127.0.0.1";

        // Check if authentication is enabled
        bool authEnabled = m_config->GetBool("Security.EnableAuthentication", false);

        if (authEnabled) {
            // Check rate limiting
            if (!m_rateLimiter->IsAllowed(clientIP,
                                         m_config->GetInt("Security.MaxAuthAttempts", 5),
                                         m_config->GetInt("Security.RateLimitWindow", 300))) {
                return false;
            }

            // Record attempt
            m_rateLimiter->RecordAttempt(clientIP);

            // Check password
            std::string serverPassword = m_config->GetString("Security.Password", "");
            if (password != serverPassword) {
                return false;
            }

            // Generate session token for successful authentication
            int tokenExpiry = m_config->GetInt("Security.SessionTokenExpiry", 3600);
            sessionToken = m_sessionManager->GenerateSessionToken(clientSocket, clientIP, tokenExpiry);

            // Reset rate limit for successful authentication
            m_rateLimiter->ResetRateLimit(clientIP);
        } else {
            // Authentication disabled - still generate a session token for tracking
            int tokenExpiry = m_config->GetInt("Security.SessionTokenExpiry", 3600);
            sessionToken = m_sessionManager->GenerateSessionToken(clientSocket, clientIP, tokenExpiry);
        }

        return true;
    }

    // Test data structures
    struct ReceivedMessage {
        SOCKET clientSocket;
        MessageType type;
        std::string payload;
    };

    // Member variables
    Config* m_config;
    SessionManager* m_sessionManager;
    RateLimiter* m_rateLimiter;
    std::unique_ptr<TCPServer> m_server;
    std::function<void(TCPServer*, SOCKET, const Message&)> m_messageHandler;
    std::vector<ReceivedMessage> m_receivedMessages;
    std::atomic<int> m_messageCount{0};
    std::mutex m_messagesMutex;
};

// Test authentication flow integration (TCPServer + SessionManager + RateLimiter)
TEST_F(IntegrationTest, AuthenticationFlowIntegration) {
    // Initialize and start server
    ASSERT_TRUE(m_server->Initialize());
    ASSERT_TRUE(m_server->Start());

    // Test successful authentication
    SOCKET clientSocket1 = 1;
    std::string sessionToken1;
    EXPECT_TRUE(SimulateClientAuthentication(clientSocket1, "test_password", sessionToken1));
    EXPECT_FALSE(sessionToken1.empty());
    EXPECT_EQ(sessionToken1.length(), 32);

    // Verify session was created
    EXPECT_TRUE(m_sessionManager->ValidateSessionToken(clientSocket1, sessionToken1));
    EXPECT_EQ(m_sessionManager->GetActiveSessionCount(), 1);
    EXPECT_EQ(m_sessionManager->GetClientIP(clientSocket1), "127.0.0.1");

    // Test failed authentication
    SOCKET clientSocket2 = 2;
    std::string sessionToken2;
    EXPECT_FALSE(SimulateClientAuthentication(clientSocket2, "wrong_password", sessionToken2));
    EXPECT_TRUE(sessionToken2.empty());

    // Verify no session was created for failed auth
    EXPECT_EQ(m_sessionManager->GetActiveSessionCount(), 1);

    // Test rate limiting after multiple failed attempts
    std::string clientIP = "127.0.0.1";
    int maxAttempts = m_config->GetInt("Security.MaxAuthAttempts", 3);

    // Make failed attempts up to the limit
    for (int i = 0; i < maxAttempts - 1; ++i) { // -1 because we already made one failed attempt
        SOCKET tempSocket = 10 + i;
        std::string tempToken;
        EXPECT_FALSE(SimulateClientAuthentication(tempSocket, "wrong_password", tempToken));
    }

    // Next attempt should be rate limited
    SOCKET clientSocket3 = 3;
    std::string sessionToken3;
    EXPECT_FALSE(SimulateClientAuthentication(clientSocket3, "test_password", sessionToken3));

    // Verify rate limiting is working
    EXPECT_FALSE(m_rateLimiter->IsAllowed(clientIP, maxAttempts, 60));

    m_server->Stop();
}

// Test session token refresh integration
TEST_F(IntegrationTest, SessionTokenRefreshIntegration) {
    // Initialize and start server
    ASSERT_TRUE(m_server->Initialize());
    ASSERT_TRUE(m_server->Start());

    // Authenticate client
    SOCKET clientSocket = 1;
    std::string originalToken;
    EXPECT_TRUE(SimulateClientAuthentication(clientSocket, "test_password", originalToken));

    // Verify original token is valid
    EXPECT_TRUE(m_sessionManager->ValidateSessionToken(clientSocket, originalToken));

    // Simulate session token refresh request
    SessionTokenRequest refreshRequest;
    refreshRequest.currentToken = originalToken;

    // Process refresh (simulating server logic)
    bool validToken = m_sessionManager->ValidateSessionToken(clientSocket, refreshRequest.currentToken);
    EXPECT_TRUE(validToken);

    if (validToken) {
        std::string clientIP = m_sessionManager->GetClientIP(clientSocket);
        int tokenExpiry = m_config->GetInt("Security.SessionTokenExpiry", 3600);
        std::string newToken = m_sessionManager->GenerateSessionToken(clientSocket, clientIP, tokenExpiry);

        // Verify new token is different and valid
        EXPECT_NE(originalToken, newToken);
        EXPECT_TRUE(m_sessionManager->ValidateSessionToken(clientSocket, newToken));

        // Original token should no longer be valid (replaced)
        EXPECT_FALSE(m_sessionManager->ValidateSessionToken(clientSocket, originalToken));
    }

    m_server->Stop();
}

// Test multiple client connections and session management
TEST_F(IntegrationTest, MultipleClientSessionManagement) {
    // Initialize and start server
    ASSERT_TRUE(m_server->Initialize());
    ASSERT_TRUE(m_server->Start());

    const int numClients = 5;
    std::vector<SOCKET> clientSockets;
    std::vector<std::string> sessionTokens;

    // Authenticate multiple clients
    for (int i = 1; i <= numClients; ++i) {
        SOCKET clientSocket = i;
        std::string sessionToken;

        EXPECT_TRUE(SimulateClientAuthentication(clientSocket, "test_password", sessionToken));
        EXPECT_FALSE(sessionToken.empty());

        clientSockets.push_back(clientSocket);
        sessionTokens.push_back(sessionToken);
    }

    // Verify all sessions are active
    EXPECT_EQ(m_sessionManager->GetActiveSessionCount(), numClients);

    // Verify each session is valid
    for (int i = 0; i < numClients; ++i) {
        EXPECT_TRUE(m_sessionManager->ValidateSessionToken(clientSockets[i], sessionTokens[i]));
        EXPECT_EQ(m_sessionManager->GetClientIP(clientSockets[i]), "127.0.0.1");
    }

    // Remove some sessions
    m_sessionManager->RemoveSession(clientSockets[1]);
    m_sessionManager->RemoveSession(clientSockets[3]);

    // Verify session count decreased
    EXPECT_EQ(m_sessionManager->GetActiveSessionCount(), numClients - 2);

    // Verify removed sessions are invalid
    EXPECT_FALSE(m_sessionManager->ValidateSessionToken(clientSockets[1], sessionTokens[1]));
    EXPECT_FALSE(m_sessionManager->ValidateSessionToken(clientSockets[3], sessionTokens[3]));

    // Verify remaining sessions are still valid
    EXPECT_TRUE(m_sessionManager->ValidateSessionToken(clientSockets[0], sessionTokens[0]));
    EXPECT_TRUE(m_sessionManager->ValidateSessionToken(clientSockets[2], sessionTokens[2]));
    EXPECT_TRUE(m_sessionManager->ValidateSessionToken(clientSockets[4], sessionTokens[4]));

    m_server->Stop();
}

// Test message processing integration
TEST_F(IntegrationTest, MessageProcessingIntegration) {
    // Initialize and start server
    ASSERT_TRUE(m_server->Initialize());
    ASSERT_TRUE(m_server->Start());

    // Authenticate a client
    SOCKET clientSocket = 1;
    std::string sessionToken;
    EXPECT_TRUE(SimulateClientAuthentication(clientSocket, "test_password", sessionToken));

    // Test heartbeat message
    Message heartbeat(MessageType::HEARTBEAT);
    HandleTestMessage(m_server.get(), clientSocket, heartbeat);

    // Verify message was received
    EXPECT_EQ(m_messageCount.load(), 1);
    EXPECT_EQ(m_receivedMessages.size(), 1);
    EXPECT_EQ(m_receivedMessages[0].clientSocket, clientSocket);
    EXPECT_EQ(m_receivedMessages[0].type, MessageType::HEARTBEAT);

    // Test quest update message
    std::string questData = "Name=Test Quest;ID=12345;Flags=1;active=true;completed=false;failed=false";
    Message questUpdate(MessageType::UPDATE_QUEST, questData);
    HandleTestMessage(m_server.get(), clientSocket, questUpdate);

    // Verify quest message was received
    EXPECT_EQ(m_messageCount.load(), 2);
    EXPECT_EQ(m_receivedMessages[1].type, MessageType::UPDATE_QUEST);
    EXPECT_EQ(m_receivedMessages[1].payload, questData);

    // Test objective update message
    std::string objectiveData = "Name=Test Quest;ID=12345;objectiveId=1;displayText=Find the item;completed=true";
    Message objectiveUpdate(MessageType::OBJECTIVE_UPDATE, objectiveData);
    HandleTestMessage(m_server.get(), clientSocket, objectiveUpdate);

    // Verify objective message was received
    EXPECT_EQ(m_messageCount.load(), 3);
    EXPECT_EQ(m_receivedMessages[2].type, MessageType::OBJECTIVE_UPDATE);
    EXPECT_EQ(m_receivedMessages[2].payload, objectiveData);

    m_server->Stop();
}

// Test configuration integration with server behavior
TEST_F(IntegrationTest, ConfigurationIntegration) {
    // Test with authentication disabled
    m_config->SetBool("Security.EnableAuthentication", false);

    ASSERT_TRUE(m_server->Initialize());
    ASSERT_TRUE(m_server->Start());

    // Should be able to "authenticate" without password
    SOCKET clientSocket1 = 1;
    std::string sessionToken1;
    EXPECT_TRUE(SimulateClientAuthentication(clientSocket1, "", sessionToken1));

    m_server->Stop();

    // Test with authentication enabled and different rate limits
    m_config->SetBool("Security.EnableAuthentication", true);
    m_config->SetString("Security.Password", "new_password");
    m_config->SetInt("Security.MaxAuthAttempts", 2);
    m_config->SetInt("Security.RateLimitWindow", 30);

    // Recreate server with new config
    m_server = std::make_unique<TCPServer>("127.0.0.1", 25580, m_messageHandler);
    ASSERT_TRUE(m_server->Initialize());
    ASSERT_TRUE(m_server->Start());

    // Test with new password
    SOCKET clientSocket2 = 2;
    std::string sessionToken2;
    EXPECT_TRUE(SimulateClientAuthentication(clientSocket2, "new_password", sessionToken2));
    EXPECT_FALSE(sessionToken2.empty());

    // Test rate limiting with new limits
    std::string clientIP = "127.0.0.1";

    // Make failed attempts up to new limit
    for (int i = 0; i < 2; ++i) {
        SOCKET tempSocket = 10 + i;
        std::string tempToken;
        EXPECT_FALSE(SimulateClientAuthentication(tempSocket, "wrong_password", tempToken));
    }

    // Should be rate limited now
    EXPECT_FALSE(m_rateLimiter->IsAllowed(clientIP, 2, 30));

    m_server->Stop();
}

// Test session expiry integration
TEST_F(IntegrationTest, SessionExpiryIntegration) {
    // Set very short session expiry for testing
    m_config->SetInt("Security.SessionTokenExpiry", 1); // 1 second

    ASSERT_TRUE(m_server->Initialize());
    ASSERT_TRUE(m_server->Start());

    // Authenticate client
    SOCKET clientSocket = 1;
    std::string sessionToken;
    EXPECT_TRUE(SimulateClientAuthentication(clientSocket, "test_password", sessionToken));

    // Verify session is initially valid
    EXPECT_TRUE(m_sessionManager->ValidateSessionToken(clientSocket, sessionToken));
    EXPECT_EQ(m_sessionManager->GetActiveSessionCount(), 1);

    // Wait for session to expire
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Session should now be invalid
    EXPECT_FALSE(m_sessionManager->ValidateSessionToken(clientSocket, sessionToken));

    // Cleanup should remove expired session
    m_sessionManager->CleanupExpiredSessions();
    EXPECT_EQ(m_sessionManager->GetActiveSessionCount(), 0);

    m_server->Stop();
}

// Test concurrent operations integration
TEST_F(IntegrationTest, ConcurrentOperationsIntegration) {
    ASSERT_TRUE(m_server->Initialize());
    ASSERT_TRUE(m_server->Start());

    const int numThreads = 10;
    const int operationsPerThread = 20;
    std::vector<std::thread> threads;
    std::atomic<int> successfulAuths{0};
    std::atomic<int> failedAuths{0};
    std::atomic<int> validTokens{0};

    // Create multiple threads performing concurrent operations
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, i, operationsPerThread, &successfulAuths, &failedAuths, &validTokens]() {
            for (int j = 0; j < operationsPerThread; ++j) {
                SOCKET clientSocket = i * operationsPerThread + j + 1;
                std::string sessionToken;

                // Authenticate client
                if (SimulateClientAuthentication(clientSocket, "test_password", sessionToken)) {
                    successfulAuths++;

                    // Validate token
                    if (m_sessionManager->ValidateSessionToken(clientSocket, sessionToken)) {
                        validTokens++;
                    }

                    // Send some messages
                    Message heartbeat(MessageType::HEARTBEAT);
                    HandleTestMessage(m_server.get(), clientSocket, heartbeat);

                    // Remove session
                    m_sessionManager->RemoveSession(clientSocket);
                } else {
                    failedAuths++;
                }

                // Small delay to allow other threads to work
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify results
    int totalOperations = numThreads * operationsPerThread;
    EXPECT_EQ(successfulAuths.load() + failedAuths.load(), totalOperations);
    EXPECT_GT(successfulAuths.load(), 0);
    EXPECT_EQ(validTokens.load(), successfulAuths.load());

    // All sessions should be removed
    EXPECT_EQ(m_sessionManager->GetActiveSessionCount(), 0);

    m_server->Stop();
}

// Test stress scenario with high load
TEST_F(IntegrationTest, StressTestIntegration) {
    ASSERT_TRUE(m_server->Initialize());
    ASSERT_TRUE(m_server->Start());

    const int numClients = 100;
    const int messagesPerClient = 10;
    std::vector<std::thread> threads;
    std::atomic<int> totalMessages{0};
    std::atomic<int> activeSessions{0};
    std::atomic<int> validSessions{0};

    // Create many clients simultaneously
    for (int i = 0; i < numClients; ++i) {
        threads.emplace_back([this, i, messagesPerClient, &totalMessages, &activeSessions, &validSessions]() {
            SOCKET clientSocket = i + 1;
            std::string sessionToken;

            // Authenticate
            if (SimulateClientAuthentication(clientSocket, "test_password", sessionToken)) {
                activeSessions++;

                // Send multiple messages
                for (int j = 0; j < messagesPerClient; ++j) {
                    std::string questData = "Name=Quest" + std::to_string(i) +
                                          ";ID=" + std::to_string(i * 1000 + j) +
                                          ";Flags=1;active=true;completed=false;failed=false";
                    Message questUpdate(MessageType::UPDATE_QUEST, questData);
                    HandleTestMessage(m_server.get(), clientSocket, questUpdate);
                    totalMessages++;

                    // Small delay between messages
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }

                // Validate session is still active (no EXPECT in thread - not thread-safe!)
                if (m_sessionManager->ValidateSessionToken(clientSocket, sessionToken)) {
                    validSessions++;
                }
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // Verify stress test results
    EXPECT_GT(activeSessions.load(), 0);
    EXPECT_EQ(totalMessages.load(), activeSessions.load() * messagesPerClient);
    EXPECT_EQ(m_sessionManager->GetActiveSessionCount(), activeSessions.load());

    // Verify message processing
    EXPECT_EQ(m_messageCount.load(), totalMessages.load());

    // Verify session validation (should match active sessions)
    EXPECT_EQ(validSessions.load(), activeSessions.load());

    m_server->Stop();
}

// Test error handling and recovery integration
TEST_F(IntegrationTest, ErrorHandlingIntegration) {
    ASSERT_TRUE(m_server->Initialize());
    ASSERT_TRUE(m_server->Start());

    // Test invalid message handling
    SOCKET clientSocket = 1;
    std::string sessionToken;
    EXPECT_TRUE(SimulateClientAuthentication(clientSocket, "test_password", sessionToken));

    // Send invalid/malformed messages
    Message invalidMessage(static_cast<MessageType>(999), "invalid_data");
    HandleTestMessage(m_server.get(), clientSocket, invalidMessage);

    // Server should handle gracefully
    EXPECT_EQ(m_messageCount.load(), 1);

    // Test session validation with invalid tokens
    EXPECT_FALSE(m_sessionManager->ValidateSessionToken(clientSocket, "invalid_token"));
    EXPECT_FALSE(m_sessionManager->ValidateSessionToken(999, sessionToken));

    // Test rate limiter with edge cases
    EXPECT_FALSE(m_rateLimiter->IsAllowed("127.0.0.1", 0, 60)); // Zero max attempts should be denied
    EXPECT_EQ(m_rateLimiter->GetRecentAttempts("127.0.0.1", 0), 0); // Zero window

    // Test cleanup operations
    m_sessionManager->CleanupExpiredSessions();
    m_rateLimiter->CleanupOldAttempts(60);

    // Session should still be valid after cleanup (not expired)
    EXPECT_TRUE(m_sessionManager->ValidateSessionToken(clientSocket, sessionToken));

    m_server->Stop();
}