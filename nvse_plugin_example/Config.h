#pragma once
#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <map>
#include <mutex>

/**
 * @brief Configuration manager class for the Quest Sync NVSE Plugin
 *
 * This class provides a thread-safe way to manage client configuration.
 * It supports loading from and saving to a configuration file, as well as
 * getting and setting configuration values at runtime.
 */
class Config {
public:
    /**
     * @brief Get the singleton instance of the configuration manager
     *
     * @return Reference to the configuration manager instance
     */
    static Config& GetInstance();

    /**
     * @brief Load configuration from a file
     *
     * @param nvseInterface The NVSE interface for getting runtime directory
     * @param filename Path to the configuration file (relative to Data/NVSE/Plugins/)
     * @return true if loading was successful, false otherwise
     */
    bool Load(const void* nvseInterface, const std::string& filename);

    /**
     * @brief Save configuration to the current file
     *
     * @return true if saving was successful, false otherwise
     */
    bool Save();

    /**
     * @brief Get a string value from the configuration
     *
     * @param key The configuration key
     * @param defaultValue The default value to return if the key is not found
     * @return The configuration value or the default value
     */
    std::string GetString(const std::string& key, const std::string& defaultValue = "");

    /**
     * @brief Get an integer value from the configuration
     *
     * @param key The configuration key
     * @param defaultValue The default value to return if the key is not found
     * @return The configuration value or the default value
     */
    int GetInt(const std::string& key, int defaultValue = 0);

    /**
     * @brief Get a boolean value from the configuration
     *
     * @param key The configuration key
     * @param defaultValue The default value to return if the key is not found
     * @return The configuration value or the default value
     */
    bool GetBool(const std::string& key, bool defaultValue = false);

    /**
     * @brief Set a string value in the configuration
     *
     * @param key The configuration key
     * @param value The value to set
     */
    void SetString(const std::string& key, const std::string& value);

    /**
     * @brief Set an integer value in the configuration
     *
     * @param key The configuration key
     * @param value The value to set
     */
    void SetInt(const std::string& key, int value);

    /**
     * @brief Set a boolean value in the configuration
     *
     * @param key The configuration key
     * @param value The value to set
     */
    void SetBool(const std::string& key, bool value);

private:
    // Private constructor for singleton
    Config();

    // Deleted copy constructor and assignment operator
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    // Trim whitespace from a string
    void Trim(std::string& str);

    // Initialize with default configuration values
    void InitializeDefaultConfig();

    // Configuration data
    std::map<std::string, std::string> m_config;

    // Current configuration file
    std::string m_filename;
    std::string m_fullPath;

    // Mutex for thread safety
    std::mutex m_mutex;
};

#endif // CONFIG_H
