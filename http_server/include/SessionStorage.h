#pragma once
#include <memory>

#include "Session.h"

class SessionStorage {
public:
    virtual ~SessionStorage() = default;
    virtual void save(std::shared_ptr<Session> session) = 0;
    virtual std::shared_ptr<Session> load(const std::string& sessionId) = 0;
    virtual void remove(const std::string& sessionId) = 0;
    virtual void clearExpired() = 0;
};

// 基于内存的会话存储实现
class MemorySessionStorage : public SessionStorage {
public:
    void save(std::shared_ptr<Session> s) override {
        std::scoped_lock lock(mutex_);
        sessions_[s->getId()] = std::move(s);
    }

    std::shared_ptr<Session> load(const std::string& id) override {
        std::scoped_lock lock(mutex_);
        auto it = sessions_.find(id);
        if (it != sessions_.end()) return it->second;
        return nullptr;
    }

    void remove(const std::string& id) override {
        std::scoped_lock lock(mutex_);
        sessions_.erase(id);
    }

    void clearExpired() override {
        std::scoped_lock lock(mutex_);
        for (auto it = sessions_.begin(); it != sessions_.end();) {
            if (it->second->isExpired()) it = sessions_.erase(it);
            else ++it;
        }
    }

private:
    std::unordered_map<std::string, std::shared_ptr<Session>> sessions_;
    std::mutex mutex_;
};
