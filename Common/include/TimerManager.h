// 高精度定时器管理：timerfd + epoll，微秒级精度
// 回调通过 ss::Signal Queued 分发到线程池，不阻塞事件循环
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "SignalSlot.h"

class TimerManager {
public:
    using TaskId   = uint64_t;
    using Callback = std::function<void()>;
    using Microseconds = std::chrono::microseconds;

    static TimerManager& instance();

    TimerManager(const TimerManager&) = delete;
    TimerManager& operator=(const TimerManager&) = delete;

    void start();
    void stop();

    TaskId addOnce(Microseconds delay, Callback cb);
    TaskId addPeriodic(Microseconds interval, Callback cb);
    void   cancel(TaskId id);
    void   cancelAll();

private:
    TimerManager();
    ~TimerManager();

    struct TimerEntry {
        int         fd;
        TaskId      id;
        bool        periodic;
        ss::Signal<> sig;
        ss::Connection conn;
    };

    void epollLoop();
    void removeEntry(TaskId id);          // 调用方须已持有 mtx_
    int  createTimerFd(Microseconds initial, Microseconds interval);

    int epoll_fd_ = -1;
    int event_fd_ = -1;                   // eventfd，用于唤醒 epoll 以退出
    std::atomic<bool> running_{false};
    std::thread worker_;

    std::mutex mtx_;
    std::unordered_map<TaskId, TimerEntry> timers_;
    TaskId next_id_ = 1;
};
