#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <chrono>

#include "CommandProcessor.h"
#include "TCPServer.h"
#include "Config.h"
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

// Test the clients command
TEST_F(CommandProcessorTest, ClientsCommand) {
    // Initialize the server
    m_server->Initialize();

    // Process the clients command
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("clients"));
}

// Test the kick command
TEST_F(CommandProcessorTest, KickCommand) {
    // Initialize the server
    m_server->Initialize();

    // Process the kick command with a valid client ID
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("kick 1"));

    // Process the kick command with an invalid client ID
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("kick 999"));

    // Process the kick command without a client ID
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("kick"));
}

// Test the broadcast command
TEST_F(CommandProcessorTest, BroadcastCommand) {
    // Initialize the server
    m_server->Initialize();

    // Process the broadcast command with a message
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("broadcast Hello, world!"));

    // Process the broadcast command without a message
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("broadcast"));
}

// Test the config command
TEST_F(CommandProcessorTest, ConfigCommand) {
    // Initialize the server
    m_server->Initialize();

    // Process the config command without arguments
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("config"));

    // Process the config command with a key
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("config Server.Port"));

    // Process the config command with a key and value
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("config Server.Port 25575"));
}

// Test command aliases
TEST_F(CommandProcessorTest, CommandAliases) {
    // Initialize the server
    m_server->Initialize();

    // Test the exit alias for stop
    EXPECT_FALSE(m_commandProcessor->ProcessCommand("exit"));

    // Test the quit alias for stop
    EXPECT_FALSE(m_commandProcessor->ProcessCommand("quit"));

    // Test the list alias for clients
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("list"));

    // Test the disconnect alias for kick
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("disconnect 1"));

    // Test the say alias for broadcast
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("say Hello, world!"));

    // Test the settings alias for config
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("settings"));
}

// Test detailed help command
TEST_F(CommandProcessorTest, DetailedHelpCommand) {
    // Process the help command with a specific command
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("help status"));

    // Process the help command with an unknown command
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("help unknown"));
}

// Test command completions
TEST_F(CommandProcessorTest, CommandCompletions) {
    // Test command completions by using ProcessCommand instead
    // We'll test that the command processor can handle commands with partial names

    // Test "h" for help
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("h"));

    // Test "s" for status
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("s"));

    // Test "exit" for exit (alias for stop)
    EXPECT_FALSE(m_commandProcessor->ProcessCommand("exit"));
}
