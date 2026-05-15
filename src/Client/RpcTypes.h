#pragma once

#include <QString>
#include "json.hpp"

struct RpcResult {
    bool ok    = false;
    int  code  = 0;
    QString msg;
    nlohmann::json data;

    static RpcResult success(nlohmann::json d) {
        return {true, 0, QString(), std::move(d)};
    }
    static RpcResult error(int c, const QString& m) {
        return {false, c, m, {}};
    }
    static RpcResult timeout() {
        return {false, -254, "timeout", {}};
    }
    static RpcResult disconnected() {
        return {false, -255, "disconnected", {}};
    }
};
