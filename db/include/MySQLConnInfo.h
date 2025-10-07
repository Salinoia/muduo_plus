#pragma once

#include <string>
struct MySQLConnInfo {
    std::string url;
    std::string user;
    std::string password;
    std::string database;
    int timeout_sec = 5;

    bool validate() const { return !url.empty() && !user.empty() && !database.empty() && timeout_sec > 0; }
};
