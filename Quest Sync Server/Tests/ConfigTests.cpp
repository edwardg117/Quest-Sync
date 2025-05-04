#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <filesystem>

#include "Config.h"
#include "MockClasses.h"

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a temporary config file for testing
        m_configFilePath = TestUtils::CreateTempFile("");
    }

    void TearDown() override {
        // Clean up the temporary config file
        TestUtils::DeleteTempFile(m_configFilePath);
    }

    // Helper function to create a config file with specific content
    void CreateConfigFile(const std::string& content) {
        std::ofstream file(m_configFilePath);
        if (file.is_open()) {
            file << content;
            file.close();
        }
    }

    // Helper function to read the config file content
    std::string ReadConfigFile() {
        std::ifstream file(m_configFilePath);
        if (!file.is_open()) {
            return "";
        }

        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();
        return content;
    }

    std::string m_configFilePath;
};

// Test loading a config file
TEST_F(ConfigTest, LoadConfig) {
    // Create a config file with some settings
    CreateConfigFile(
        "Server.IpAddress=127.0.0.1\n"
        "Server.Port=25575\n"
        "Logging.ConsoleLevel=INFO\n"
        "Logging.FileLevel=DEBUG\n"
        "Logging.LogFile=server.log\n"
    );

    // Load the config
    Config& config = Config::GetInstance();
    EXPECT_TRUE(config.Load(m_configFilePath));

    // Check if the settings were loaded correctly
    EXPECT_EQ(config.GetString("Server.IpAddress", ""), "127.0.0.1");
    EXPECT_EQ(config.GetInt("Server.Port", 0), 25575);
    EXPECT_EQ(config.GetString("Logging.ConsoleLevel", ""), "INFO");
    EXPECT_EQ(config.GetString("Logging.FileLevel", ""), "DEBUG");
    EXPECT_EQ(config.GetString("Logging.LogFile", ""), "server.log");
}

// Test creating a default config file
TEST_F(ConfigTest, CreateDefaultConfig) {
    // Delete the config file if it exists
    TestUtils::DeleteTempFile(m_configFilePath);

    // Load the config (should create a default one)
    Config& config = Config::GetInstance();
    EXPECT_TRUE(config.Load(m_configFilePath));

    // Check if the default settings were created
    EXPECT_FALSE(config.GetString("Server.IpAddress", "default").empty());
    EXPECT_NE(config.GetInt("Server.Port", 0), 0);
    EXPECT_FALSE(config.GetString("Logging.ConsoleLevel", "").empty());
    EXPECT_FALSE(config.GetString("Logging.FileLevel", "").empty());
    EXPECT_FALSE(config.GetString("Logging.LogFile", "").empty());

    // Check if the file was created
    std::ifstream checkFile(m_configFilePath);
    EXPECT_TRUE(checkFile.good());
}

// Test saving config changes
TEST_F(ConfigTest, SaveConfig) {
    // Create a config file with some settings
    CreateConfigFile(
        "Server.IpAddress=127.0.0.1\n"
        "Server.Port=25575\n"
    );

    // Load the config
    Config& config = Config::GetInstance();
    EXPECT_TRUE(config.Load(m_configFilePath));

    // Change some settings
    config.SetString("Server.IpAddress", "192.168.1.1");
    config.SetInt("Server.Port", 8080);
    config.SetString("NewSetting", "TestValue");

    // Save the config
    EXPECT_TRUE(config.Save());

    // Check if the changes were saved to the file
    std::string content = ReadConfigFile();
    EXPECT_NE(content.find("Server.IpAddress=192.168.1.1"), std::string::npos);
    EXPECT_NE(content.find("Server.Port=8080"), std::string::npos);
    EXPECT_NE(content.find("NewSetting=TestValue"), std::string::npos);
}

// Test getting settings with default values
TEST_F(ConfigTest, GetWithDefaults) {
    // Create an empty config file
    CreateConfigFile("");

    // Load the config
    Config& config = Config::GetInstance();
    EXPECT_TRUE(config.Load(m_configFilePath));

    // Get settings with default values
    EXPECT_EQ(config.GetString("NonExistentSetting", "DefaultValue"), "DefaultValue");
    EXPECT_EQ(config.GetInt("NonExistentSetting", 42), 42);
    EXPECT_EQ(config.GetBool("NonExistentSetting", true), true);
}

// Test boolean settings
TEST_F(ConfigTest, BooleanSettings) {
    // Create a config file with boolean settings
    CreateConfigFile(
        "Setting1=true\n"
        "Setting2=false\n"
        "Setting3=1\n"
        "Setting4=0\n"
        "Setting5=yes\n"
        "Setting6=no\n"
    );

    // Load the config
    Config& config = Config::GetInstance();
    EXPECT_TRUE(config.Load(m_configFilePath));

    // Check if the boolean settings were loaded correctly
    EXPECT_TRUE(config.GetBool("Setting1", false));
    EXPECT_FALSE(config.GetBool("Setting2", true));
    EXPECT_TRUE(config.GetBool("Setting3", false));
    EXPECT_FALSE(config.GetBool("Setting4", true));
    EXPECT_TRUE(config.GetBool("Setting5", false));
    EXPECT_FALSE(config.GetBool("Setting6", true));
}

// Test updating missing settings
TEST_F(ConfigTest, UpdateMissingSettings) {
    // Create a config file with some settings
    CreateConfigFile(
        "Server.IpAddress=127.0.0.1\n"
        "Server.Port=25575\n"
    );

    // Load the config
    Config& config = Config::GetInstance();
    EXPECT_TRUE(config.Load(m_configFilePath));

    // Check if missing settings were added with default values
    EXPECT_FALSE(config.GetString("Logging.ConsoleLevel", "").empty());
    EXPECT_FALSE(config.GetString("Logging.FileLevel", "").empty());
    EXPECT_FALSE(config.GetString("Logging.LogFile", "").empty());

    // Save the config
    EXPECT_TRUE(config.Save());

    // Check if the missing settings were saved to the file
    std::string content = ReadConfigFile();
    EXPECT_NE(content.find("Logging.ConsoleLevel"), std::string::npos);
    EXPECT_NE(content.find("Logging.FileLevel"), std::string::npos);
    EXPECT_NE(content.find("Logging.LogFile"), std::string::npos);
}

// Test thread safety
TEST_F(ConfigTest, ThreadSafety) {
    // Create a config file with some settings
    CreateConfigFile(
        "Server.IpAddress=127.0.0.1\n"
        "Server.Port=25575\n"
    );

    // Load the config
    Config& config = Config::GetInstance();
    EXPECT_TRUE(config.Load(m_configFilePath));

    // Create multiple threads that read and write settings
    const int numThreads = 10;
    const int numOperationsPerThread = 100;

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([i, numOperationsPerThread, &config]() {
            for (int j = 0; j < numOperationsPerThread; ++j) {
                // Read a setting
                config.GetString("Server.IpAddress", "");
                config.GetInt("Server.Port", 0);

                // Write a setting
                config.SetString("Thread" + std::to_string(i) + ".Setting" + std::to_string(j), "Value");
                config.SetInt("Thread" + std::to_string(i) + ".Count", j);
            }
        });
    }

    // Wait for all threads to finish
    for (auto& thread : threads) {
        thread.join();
    }

    // Save the config
    EXPECT_TRUE(config.Save());

    // Check if the settings from all threads were saved
    for (int i = 0; i < numThreads; ++i) {
        for (int j = 0; j < numOperationsPerThread; ++j) {
            EXPECT_EQ(config.GetString("Thread" + std::to_string(i) + ".Setting" + std::to_string(j), ""), "Value");
        }
        EXPECT_EQ(config.GetInt("Thread" + std::to_string(i) + ".Count", -1), numOperationsPerThread - 1);
    }
}
