#include "ConfigManager.h"
#include <fstream>
#include <sstream>
#include <cctype>
#include <chrono>

namespace {
std::string Trim(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}
}

ConfigManager& ConfigManager::Instance() {
    static ConfigManager instance;
    return instance;
}

ConfigManager::~ConfigManager() { StopWatch(); }

bool ConfigManager::Load(const std::string& filename) {
    std::ifstream ifs(filename);
    if (!ifs.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    bool ok = false;
    if (filename.size() >= 5) {
        auto ext = filename.substr(filename.find_last_of('.') + 1);
        if (ext == "json") ok = ParseJson(content);
        else if (ext == "yml" || ext == "yaml") ok = ParseYaml(content);
    }
    if (!ok) ok = ParseJson(content) || ParseYaml(content);
    if (ok) {
        filename_ = filename;
        lastWriteTime_ = std::filesystem::last_write_time(filename_);
    }
    return ok;
}

void ConfigManager::StartWatch(int intervalMs) {
    StopWatch();
    if (filename_.empty()) return;
    watching_ = true;
    watcher_ = std::thread(&ConfigManager::WatchLoop, this, intervalMs);
}

void ConfigManager::StopWatch() {
    watching_ = false;
    if (watcher_.joinable()) watcher_.join();
}

void ConfigManager::WatchLoop(int intervalMs) {
    while (watching_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
        auto cur = std::filesystem::last_write_time(filename_);
        if (cur != lastWriteTime_) {
            Load(filename_);
        }
    }
}

bool ConfigManager::ParseJson(const std::string& content) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.clear();
    size_t pos = 0;
    while (true) {
        size_t keyStart = content.find('"', pos);
        if (keyStart == std::string::npos) break;
        size_t keyEnd = content.find('"', keyStart + 1);
        if (keyEnd == std::string::npos) break;
        std::string key = content.substr(keyStart + 1, keyEnd - keyStart - 1);
        size_t colon = content.find(':', keyEnd);
        if (colon == std::string::npos) break;
        size_t valueStart = colon + 1;
        while (valueStart < content.size() && std::isspace(static_cast<unsigned char>(content[valueStart]))) ++valueStart;
        size_t valueEnd = valueStart;
        if (valueStart < content.size() && content[valueStart] == '"') {
            ++valueStart;
            valueEnd = content.find('"', valueStart);
            if (valueEnd == std::string::npos) break;
            std::string value = content.substr(valueStart, valueEnd - valueStart);
            data_[key] = value;
            pos = valueEnd + 1;
        } else {
            while (valueEnd < content.size() && content[valueEnd] != ',' && content[valueEnd] != '}' && content[valueEnd] != '\n') ++valueEnd;
            std::string value = content.substr(valueStart, valueEnd - valueStart);
            data_[key] = Trim(value);
            pos = valueEnd + 1;
        }
    }
    return !data_.empty();
}

bool ConfigManager::ParseYaml(const std::string& content) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_.clear();
    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        auto hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        line = Trim(line);
        if (line.empty()) continue;
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = Trim(line.substr(0, colon));
        std::string value = Trim(line.substr(colon + 1));
        if (!value.empty() && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }
        data_[key] = value;
    }
    return !data_.empty();
}

std::string ConfigManager::GetString(const std::string& key, const std::string& defaultValue) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = data_.find(key);
    return it != data_.end() ? it->second : defaultValue;
}

int ConfigManager::GetInt(const std::string& key, int defaultValue) const {
    try {
        return std::stoi(GetString(key));
    } catch (...) {
        return defaultValue;
    }
}

double ConfigManager::GetDouble(const std::string& key, double defaultValue) const {
    try {
        return std::stod(GetString(key));
    } catch (...) {
        return defaultValue;
    }
}

bool ConfigManager::GetBool(const std::string& key, bool defaultValue) const {
    auto val = GetString(key);
    if (val == "true" || val == "1") return true;
    if (val == "false" || val == "0") return false;
    return defaultValue;
}

