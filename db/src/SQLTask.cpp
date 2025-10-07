#include "SQLTask.h"

#include <cppconn/exception.h>

#include <iostream>

#include "MySQLConn.h"

// ------------------ SQLOperation ------------------
void SQLOperation::Execute(MySQLConn* conn) {
    try {
        if (kind_ == SQLKind::Query) {
            promise_.set_value(conn->ExecuteQuery(sql_));
        } else if (kind_ == SQLKind::Exec) {
            promise_bool_.set_value(conn->ExecuteStatement(sql_));
        } else if (kind_ == SQLKind::Update) {
            promise_int_.set_value(conn->ExecuteUpdate(sql_));
        }
    } catch (...) {
        // 出错时返回默认值
        if (kind_ == SQLKind::Query) promise_.set_value(nullptr);
        else if (kind_ == SQLKind::Exec) promise_bool_.set_value(false);
        else promise_int_.set_value(-1);
    }
}
