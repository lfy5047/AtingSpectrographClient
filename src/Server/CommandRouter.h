#pragma once

#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "json.hpp"

namespace srv {

// 处理失败时由 handler 抛出。code/msg 直接进入错误响应。
class RpcError : public std::runtime_error {
public:
    RpcError(int code, std::string msg)
        : std::runtime_error(msg), code_(code) {}
    int code() const { return code_; }
private:
    int code_;
};

// 命令路由表：cmd 字符串 -> handler。handler 入参为 params(json)，返回 data(json)。
// dispatch() 始终返回标准响应 payload：
//   成功：{"ok": true,  "data": <handler returned json>}
//   失败：{"ok": false, "code": <int>, "msg": <string>}
class CommandRouter {
public:
    using Handler = std::function<nlohmann::json(const nlohmann::json& params)>;

    void registerCmd(std::string name, Handler h) {
        std::lock_guard<std::mutex> lk(mtx_);
        handlers_[std::move(name)] = std::move(h);
    }

    bool has(const std::string& name) const {
        std::lock_guard<std::mutex> lk(mtx_);
        return handlers_.count(name) > 0;
    }

    nlohmann::json dispatch(const std::string& cmd, const nlohmann::json& params) {
        Handler h;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = handlers_.find(cmd);
            if (it != handlers_.end()) h = it->second;
        }
        if (!h) {
            return errorResp(-1, "unknown cmd: " + cmd);
        }
        try {
            return okResp(h(params));
        } catch (const RpcError& e) {
            return errorResp(e.code(), e.what());
        } catch (const nlohmann::json::exception& e) {
            return errorResp(-3, std::string("bad params: ") + e.what());
        } catch (const std::exception& e) {
            return errorResp(-100, std::string("exception: ") + e.what());
        }
    }

    static nlohmann::json okResp(nlohmann::json data) {
        return nlohmann::json{{"ok", true}, {"data", std::move(data)}};
    }
    static nlohmann::json errorResp(int code, std::string msg) {
        return nlohmann::json{{"ok", false}, {"code", code}, {"msg", std::move(msg)}};
    }

private:
    mutable std::mutex mtx_;
    std::unordered_map<std::string, Handler> handlers_;
};

} // namespace srv
