#pragma once
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

class SessionManager;

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(const std::string& sessionId, std::weak_ptr<SessionManager> manager, int maxAge = 3600);

    const std::string& getId() const { return sessionId_; }

    bool isExpired() const;
    void refresh();

    void setValue(const std::string& key, const std::string& value);
    std::string getValue(const std::string& key) const;
    void remove(const std::string& key);
    void clear();

    void setManager(std::weak_ptr<SessionManager> mgr) { manager_ = std::move(mgr); }
    std::weak_ptr<SessionManager> getManager() const { return manager_; }

private:
    std::string sessionId_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::string> data_;

    std::chrono::system_clock::time_point expiryTime_;
    int maxAge_;

    std::weak_ptr<SessionManager> manager_;
};
