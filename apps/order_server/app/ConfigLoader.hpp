#pragma once
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <yaml-cpp/yaml.h>

class ConfigLoader {
public:
    explicit ConfigLoader(const std::string& filePath) {
        try {
            config_ = std::make_shared<YAML::Node>(YAML::LoadFile(filePath));
            std::cerr << "[Debug] ConfigLoader constructed at " << this << std::endl;
        } catch (const std::exception& e) {
            throw std::runtime_error("Failed to load config file: " + std::string(e.what()));
        }
    }

    // 获取标量值（支持默认值）
    template <typename T>
    T get(const std::string& key, const T& defaultValue) const {
        try {
            if ((*config_)[key])
                return (*config_)[key].as<T>();
        } catch (...) {}
        return defaultValue;
    }

    // 分层路径读取
    template <typename T>
    T getPath(const std::string& path, const T& defaultValue) const {
        try {
            YAML::Node root = YAML::Clone(*config_);  // 独立副本
            YAML::Node node = root;
            std::stringstream ss(path);
            std::string segment;

            while (std::getline(ss, segment, '.')) {
                if (!node.IsMap()) return defaultValue;
                node = node[segment];
                if (!node.IsDefined()) return defaultValue;
            }
            return node.as<T>();
        } catch (...) {
            return defaultValue;
        }
    }


    YAML::Node operator[](const std::string& key) const { return (*config_)[key]; }
    bool has(const std::string& key) const { return (*config_)[key] && !(*config_)[key].IsNull(); }

    void dump(std::ostream& os = std::cout) const { os << YAML::Dump(*config_) << std::endl; }

private:
    std::shared_ptr<YAML::Node> config_;
};
