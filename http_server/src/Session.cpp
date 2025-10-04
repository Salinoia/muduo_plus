#include "Session.h"

#include <chrono>

#include "SessionManager.h"

Session::Session(const std::string& sessionId, std::weak_ptr<SessionManager> manager, int maxAge) : sessionId_(sessionId), maxAge_(maxAge), manager_(std::move(manager)) {
    refresh();
}

// 检查会话是否已过期
bool Session::isExpired() const {
    return std::chrono::system_clock::now() > expiryTime_;
}

// 刷新会话的过期时间
void Session::refresh() {
    expiryTime_ = std::chrono::system_clock::now() + std::chrono::seconds(maxAge_);
}

// 设置键值
void Session::setValue(const std::string& key, const std::string& value) {
    {
        std::scoped_lock lock(mutex_);
        data_[key] = value;
    }

    if (auto mgr = manager_.lock()) {
        mgr->updateSession(shared_from_this());
    }
}

// 获取键值
std::string Session::getValue(const std::string& key) const {
    std::scoped_lock lock(mutex_);
    auto it = data_.find(key);
    return it != data_.end() ? it->second : std::string();
}

// 删除键
void Session::remove(const std::string& key) {
    {
        std::scoped_lock lock(mutex_);
        data_.erase(key);
    }

    if (auto mgr = manager_.lock()) {
        mgr->updateSession(shared_from_this());
    }
}

// 清空所有数据
void Session::clear() {
    {
        std::scoped_lock lock(mutex_);
        data_.clear();
    }

    if (auto mgr = manager_.lock()) {
        mgr->updateSession(shared_from_this());
    }
}
