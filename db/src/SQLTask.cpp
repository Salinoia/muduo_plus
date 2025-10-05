#include "SQLTask.h"

#include <cppconn/exception.h>

#include <iostream>

#include "MySQLConn.h"

// ------------------ SQLOperation ------------------
void SQLOperation::Execute(MySQLConn* conn) {
    try {
        if (kind_ == SQLKind::Query) {
            auto rs = conn->ExecuteQuery(sql_);
            promise_.set_value(std::move(rs));
        } else {  // Exec 类型（INSERT/UPDATE/DELETE）
            conn->ExecuteUpdate(sql_);
            promise_.set_value(nullptr);  // 无返回结果集
        }
    } catch (const sql::SQLException& e) {
        promise_.set_exception(std::make_exception_ptr(e));
    }
}
