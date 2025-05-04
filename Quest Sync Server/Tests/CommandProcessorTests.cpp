#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <chrono>

#include "CommandProcessor.h"
#include "TCPServer.h"
#include "MockClasses.h"

class CommandProcessorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a TCP server
        m_server = new TCPServer("127.0.0.1", 25575, [](TCPServer*, SOCKET, const Message&) {});

        // Create the command processor
        m_commandProcessor = new CommandProcessor(m_server);
    }

    void TearDown() override {
        // Clean up
        delete m_commandProcessor;
        delete m_server;
    }

    TCPServer* m_server;
    CommandProcessor* m_commandProcessor;
};

// Test command processor initialization
TEST_F(CommandProcessorTest, Initialization) {
    // Check if the command processor is not running initially
    EXPECT_FALSE(m_commandProcessor->IsRunning());
}

// Test the help command
TEST_F(CommandProcessorTest, HelpCommand) {
    // Process the help command
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("help"));
}

// Test the stop command
TEST_F(CommandProcessorTest, StopCommand) {
    // Process the stop command
    EXPECT_FALSE(m_commandProcessor->ProcessCommand("stop"));
}

// Test the status command
TEST_F(CommandProcessorTest, StatusCommand) {
    // Initialize the server
    m_server->Initialize();

    // Process the status command
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("status"));
}

// Test an unknown command
TEST_F(CommandProcessorTest, UnknownCommand) {
    // Process an unknown command
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("unknown"));
}

// Test command with arguments
TEST_F(CommandProcessorTest, CommandWithArguments) {
    // Process a command with arguments
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("help status"));
}

// Test empty command
TEST_F(CommandProcessorTest, EmptyCommand) {
    // Process an empty command
    EXPECT_TRUE(m_commandProcessor->ProcessCommand(""));
}
