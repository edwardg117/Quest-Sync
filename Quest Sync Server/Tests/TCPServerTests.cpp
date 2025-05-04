#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <chrono>
#include <WS2tcpip.h>

#include "TCPServer.h"
#include "Message.h"
#include "MockClasses.h"

// Simple message handler for testing
void TestMessageHandler(TCPServer* server, SOCKET client, const Message& message) {
    // Just a stub implementation
}

class TCPServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create the server with the message handler
        m_server = new TCPServer("127.0.0.1", 25575, TestMessageHandler);
    }

    void TearDown() override {
        // Clean up
        delete m_server;
    }

    TCPServer* m_server;
};

// Test server initialization
TEST_F(TCPServerTest, Initialize) {
    // Initialize the server
    EXPECT_TRUE(m_server->Initialize());
}

// Test server start and stop
TEST_F(TCPServerTest, StartStop) {
    // Initialize the server
    EXPECT_TRUE(m_server->Initialize());

    // Start the server
    EXPECT_TRUE(m_server->Start());

    // Wait a bit for the server to start
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Check if the server is running
    EXPECT_TRUE(m_server->IsRunning());

    // Stop the server
    m_server->Stop();

    // Wait a bit for the server to stop
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Check if the server has stopped
    EXPECT_FALSE(m_server->IsRunning());
}

// Test sending messages
TEST_F(TCPServerTest, SendMessage) {
    // Initialize the server
    EXPECT_TRUE(m_server->Initialize());

    // Create a test message
    Message testMessage(MessageType::HANDSHAKE_RESPONSE, "Test payload");

    // Send a message to a client (this will fail in the test environment, but we're just testing the API)
    m_server->SendToClient(2, testMessage);
}

// Test broadcasting messages
TEST_F(TCPServerTest, BroadcastMessage) {
    // Initialize the server
    EXPECT_TRUE(m_server->Initialize());

    // Create a test message
    Message testMessage(MessageType::EVENT_NOTIFICATION, "Test payload");

    // Broadcast a message to all clients (this will do nothing in the test environment)
    m_server->BroadcastMessage(testMessage, INVALID_SOCKET);
}
