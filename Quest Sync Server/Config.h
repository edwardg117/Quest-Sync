#pragma once
#ifndef CONFIG_H
#define CONFIG_H

#include <string>
#include <map>
#include <mutex>

/**
 * @brief Configuration manager class
 *
 * This class provides a thread-safe way to manage application configuration.
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
     * @param filename Path to the configuration file
     * @return true if loading was successful, false otherwise
     */
    bool Load(const std::string& filename);

    /**
     * @brief Save configuration to a file
     *
     * @param filename Path to the configuration file
     * @return true if saving was successful, false otherwise
     */
    bool Save(const std::string& filename = "");

    /**
     * @brief Get a configuration value
     *
     * @param key Configuration key
     * @param defaultValue Default value to return if key is not found
     * @return Configuration value or default value if key is not found
     */
    std::string GetString(const std::string& key, const std::string& defaultValue = "");

    /**
     * @brief Get a configuration value as an integer
     *
     * @param key Configuration key
     * @param defaultValue Default value to return if key is not found or value is not an integer
     * @return Configuration value as integer or default value
     */
    int GetInt(const std::string& key, int defaultValue = 0);

    /**
     * @brief Get a configuration value as a boolean
     *
     * @param key Configuration key
     * @param defaultValue Default value to return if key is not found or value is not a boolean
     * @return Configuration value as boolean or default value
     */
    bool GetBool(const std::string& key, bool defaultValue = false);

    /**
     * @brief Set a configuration value
     *
     * @param key Configuration key
     * @param value Configuration value
     */
    void SetString(const std::string& key, const std::string& value);

    /**
     * @brief Set a configuration value as an integer
     *
     * @param key Configuration key
     * @param value Configuration value
     */
    void SetInt(const std::string& key, int value);

    /**
     * @brief Set a configuration value as a boolean
     *
     * @param key Configuration key
     * @param value Configuration value
     */
    void SetBool(const std::string& key, bool value);

    /**
     * @brief Check if the configuration has all default settings
     *
     * @return true if all default settings are present, false otherwise
     */
    bool HasAllDefaultSettings();

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

    // Mutex for thread safety
    std::mutex m_mutex;
};

#endif // CONFIG_H
