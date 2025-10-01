#pragma once
#include <string>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <filesystem>

// Simple configuration manager supporting JSON and YAML (flat key-value).
// It loads configuration files and provides typed getters. It can optionally
// watch for file changes and reload automatically.
class ConfigManager {
public:
    static ConfigManager& Instance();

    // Load configuration from a JSON or YAML file.
    bool Load(const std::string& filename);

    // Start/stop watching the loaded file for modifications.
    void StartWatch(int intervalMs = 1000);
    void StopWatch();

    std::string GetString(const std::string& key, const std::string& defaultValue = "") const;
    int GetInt(const std::string& key, int defaultValue = 0) const;
    double GetDouble(const std::string& key, double defaultValue = 0.0) const;
    bool GetBool(const std::string& key, bool defaultValue = false) const;

    ~ConfigManager();

private:
    ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    bool ParseJson(const std::string& content);
    bool ParseYaml(const std::string& content);
    void WatchLoop(int intervalMs);

    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::string> data_;
    std::string filename_;
    std::filesystem::file_time_type lastWriteTime_{};

    bool watching_ = false;
    std::thread watcher_;
};

