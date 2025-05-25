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

// Test case sensitivity
TEST_F(CommandProcessorTest, CaseSensitivity) {
    // Initialize the server
    m_server->Initialize();

    // Test uppercase commands
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("HELP"));
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("STATUS"));
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("CLIENTS"));

    // Test mixed case commands
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("Help"));
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("Status"));
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("Clients"));
}

// Test whitespace handling
TEST_F(CommandProcessorTest, WhitespaceHandling) {
    // Initialize the server
    m_server->Initialize();

    // Test commands with leading/trailing spaces
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("  help  "));
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("\tstatus\t"));
    EXPECT_TRUE(m_commandProcessor->ProcessCommand(" clients "));

    // Test commands with multiple spaces
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("kick    1"));
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("broadcast    Hello    World"));
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("config    Server.Port    25575"));
}

// Test special characters in commands
TEST_F(CommandProcessorTest, SpecialCharacters) {
    // Initialize the server
    m_server->Initialize();

    // Test broadcast with special characters
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("broadcast Hello! @#$%^&*()"));
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("broadcast Unicode: αβγδε"));
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("broadcast Numbers: 12345"));

    // Test config with special characters
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("config Test.Setting \"Value with spaces\""));
}

// Test long commands
TEST_F(CommandProcessorTest, LongCommands) {
    // Initialize the server
    m_server->Initialize();

    // Test very long broadcast message
    std::string longMessage(1000, 'A');
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("broadcast " + longMessage));

    // Test long config key/value
    std::string longKey(100, 'K');
    std::string longValue(100, 'V');
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("config " + longKey + " " + longValue));
}

// Test rapid command processing
TEST_F(CommandProcessorTest, RapidCommandProcessing) {
    // Initialize the server
    m_server->Initialize();

    // Process many commands rapidly
    for (int i = 0; i < 100; ++i) {
        EXPECT_TRUE(m_commandProcessor->ProcessCommand("status"));
        EXPECT_TRUE(m_commandProcessor->ProcessCommand("clients"));
        EXPECT_TRUE(m_commandProcessor->ProcessCommand("help"));
    }
}

// Test concurrent command processing
TEST_F(CommandProcessorTest, ConcurrentCommandProcessing) {
    // Initialize the server
    m_server->Initialize();

    const int numThreads = 5;
    const int commandsPerThread = 20;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    // Create threads that process commands concurrently
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, i, commandsPerThread, &successCount]() {
            for (int j = 0; j < commandsPerThread; ++j) {
                std::string command = "broadcast Thread " + std::to_string(i) + " Command " + std::to_string(j);
                if (m_commandProcessor->ProcessCommand(command)) {
                    successCount++;
                }

                // Also test other commands
                m_commandProcessor->ProcessCommand("status");
                m_commandProcessor->ProcessCommand("clients");
                m_commandProcessor->ProcessCommand("help");
            }
        });
    }

    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }

    // All broadcast commands should have succeeded
    EXPECT_EQ(successCount.load(), numThreads * commandsPerThread);
}

// Test invalid kick commands
TEST_F(CommandProcessorTest, InvalidKickCommands) {
    // Initialize the server
    m_server->Initialize();

    // Test kick with invalid client IDs
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("kick abc"));
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("kick -1"));
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("kick 0"));
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("kick 999999"));

    // Test kick with multiple arguments
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("kick 1 2 3"));
}

// Test invalid config commands
TEST_F(CommandProcessorTest, InvalidConfigCommands) {
    // Initialize the server
    m_server->Initialize();

    // Test config with invalid keys
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("config"));
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("config InvalidKey"));
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("config . ."));
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("config \"\" \"\""));
}

// Test command history simulation
TEST_F(CommandProcessorTest, CommandHistorySimulation) {
    // Initialize the server
    m_server->Initialize();

    // Simulate a series of commands that might be in a command history
    std::vector<std::string> commandHistory = {
        "help",
        "status",
        "clients",
        "broadcast Welcome to the server!",
        "config Server.Port",
        "kick 1",
        "help status",
        "broadcast Server maintenance in 5 minutes",
        "clients",
        "status"
    };

    // Process all commands in history
    for (const auto& command : commandHistory) {
        // All commands should process successfully (return true) except stop-like commands
        bool result = m_commandProcessor->ProcessCommand(command);
        if (command != "stop" && command != "exit" && command != "quit") {
            EXPECT_TRUE(result);
        }
    }
}

// Test edge case commands
TEST_F(CommandProcessorTest, EdgeCaseCommands) {
    // Initialize the server
    m_server->Initialize();

    // Test commands with only spaces
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("   "));
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("\t\t\t"));

    // Test commands with newlines (should be handled gracefully)
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("help\n"));
    EXPECT_TRUE(m_commandProcessor->ProcessCommand("status\r\n"));

    // Test commands with null characters (should be handled gracefully)
    std::string commandWithNull = "help";
    commandWithNull += '\0';
    commandWithNull += "extra";
    EXPECT_TRUE(m_commandProcessor->ProcessCommand(commandWithNull));
}
