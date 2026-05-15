#include "TimerManager.h"

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <cstring>
#include <stdexcept>

static constexpr int MAX_EVENTS = 64;

static itimerspec usToSpec(TimerManager::Microseconds initial,
                           TimerManager::Microseconds interval) {
    auto toTs = [](TimerManager::Microseconds us) -> timespec {
        auto s  = std::chrono::duration_cast<std::chrono::seconds>(us);
        auto rem = us - std::chrono::duration_cast<TimerManager::Microseconds>(s);
        return {static_cast<time_t>(s.count()),
                static_cast<long>(rem.count()) * 1000L};  // us -> ns
    };
    return {toTs(interval), toTs(initial)};
}

TimerManager& TimerManager::instance() {
    static TimerManager tm;
    return tm;
}

// ─── 构造 / 析构 ────────────────────────────────────────────────────────────

TimerManager::TimerManager() {
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ < 0)
        throw std::runtime_error("epoll_create1 failed");

    event_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (event_fd_ < 0) {
        close(epoll_fd_);
        throw std::runtime_error("eventfd failed");
    }

    epoll_event ev{};
    ev.events   = EPOLLIN;
    ev.data.u64 = 0;              // TaskId 从 1 开始，0 专属 event_fd_
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, event_fd_, &ev);
    start();
}

TimerManager::~TimerManager() {
    stop();
    close(event_fd_);
    close(epoll_fd_);
}

// ─── start / stop ───────────────────────────────────────────────────────────

void TimerManager::start() {
    if (running_.exchange(true)) return;
    worker_ = std::thread(&TimerManager::epollLoop, this);
}

void TimerManager::stop() {
    if (!running_.exchange(false)) return;

    // 写 eventfd 唤醒 epoll_wait
    uint64_t one = 1;
    ::write(event_fd_, &one, sizeof(one));

    if (worker_.joinable()) worker_.join();
    cancelAll();
}

// ─── 添加任务 ───────────────────────────────────────────────────────────────

TimerManager::TaskId TimerManager::addOnce(Microseconds delay, Callback cb) {
    int fd = createTimerFd(delay, Microseconds::zero());

    std::lock_guard<std::mutex> lk(mtx_);
    TaskId id = next_id_++;
    auto& entry  = timers_[id];
    entry.fd       = fd;
    entry.id       = id;
    entry.periodic = false;
    entry.conn     = entry.sig.connect(std::move(cb), ss::ConnType::Queued);

    epoll_event ev{};
    ev.events  = EPOLLIN;
    ev.data.u64 = id;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev);
    return id;
}

TimerManager::TaskId TimerManager::addPeriodic(Microseconds interval, Callback cb) {
    int fd = createTimerFd(interval, interval);

    std::lock_guard<std::mutex> lk(mtx_);
    TaskId id = next_id_++;
    auto& entry  = timers_[id];
    entry.fd       = fd;
    entry.id       = id;
    entry.periodic = true;
    entry.conn     = entry.sig.connect(std::move(cb), ss::ConnType::Queued);

    epoll_event ev{};
    ev.events  = EPOLLIN;
    ev.data.u64 = id;
    epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev);
    return id;
}

// ─── 取消 ───────────────────────────────────────────────────────────────────

void TimerManager::cancel(TaskId id) {
    std::lock_guard<std::mutex> lk(mtx_);
    removeEntry(id);
}

void TimerManager::cancelAll() {
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto it = timers_.begin(); it != timers_.end();) {
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, it->second.fd, nullptr);
        close(it->second.fd);
        it = timers_.erase(it);
    }
}

void TimerManager::removeEntry(TaskId id) {
    auto it = timers_.find(id);
    if (it == timers_.end()) return;
    epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, it->second.fd, nullptr);
    close(it->second.fd);
    timers_.erase(it);
}

// ─── epoll 事件循环 ─────────────────────────────────────────────────────────

void TimerManager::epollLoop() {
    epoll_event events[MAX_EVENTS];
    while (running_) {
        int n = epoll_wait(epoll_fd_, events, MAX_EVENTS, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        for (int i = 0; i < n; ++i) {
            TaskId tid = events[i].data.u64;
            if (tid == 0) {
                uint64_t tmp;
                ::read(event_fd_, &tmp, sizeof(tmp));
                continue;
            }
            std::lock_guard<std::mutex> lk(mtx_);
            auto it = timers_.find(tid);
            if (it == timers_.end()) continue;

            // 消费 timerfd 到期计数
            uint64_t expirations = 0;
            ::read(it->second.fd, &expirations, sizeof(expirations));

            it->second.sig.emit();

            if (!it->second.periodic)
                removeEntry(tid);
        }
    }
}

// ─── 内部工具 ───────────────────────────────────────────────────────────────

int TimerManager::createTimerFd(Microseconds initial, Microseconds interval) {
    int fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (fd < 0)
        throw std::runtime_error("timerfd_create failed");

    itimerspec spec = usToSpec(initial, interval);
    if (timerfd_settime(fd, 0, &spec, nullptr) < 0) {
        close(fd);
        throw std::runtime_error("timerfd_settime failed");
    }
    return fd;
}
