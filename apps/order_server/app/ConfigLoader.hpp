#pragma once
#include <string>
#include <yaml-cpp/yaml.h>
#include <iostream>
#include <stdexcept>

class ConfigLoader {
public:
    explicit ConfigLoader(const std::string& filePath) {
        try {
            config_ = YAML::LoadFile(filePath);
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to load config file: " + std::string(e.what()));
        }
    }

    // 获取标量值（支持默认值）
    template <typename T>
    T get(const std::string& key, const T& defaultValue) const {
        if (config_[key])
            return config_[key].as<T>();
        return defaultValue;
    }

    template <typename T>
    T getPath(const std::string& path, const T& defaultValue) const {
        YAML::Node node = config_;
        std::stringstream ss(path);
        std::string segment;
        while (std::getline(ss, segment, '.')) {
            if (!node[segment])
                return defaultValue;
            node = node[segment];
        }
        return node.as<T>();
    }

    // 获取嵌套节点
    YAML::Node operator[](const std::string& key) const { return config_[key]; }

    // 判断键是否存在
    bool has(const std::string& key) const { return config_[key] && !config_[key].IsNull(); }

    void dump(std::ostream& os = std::cout) const { os << YAML::Dump(config_) << std::endl; }

private:
    YAML::Node config_;
};
