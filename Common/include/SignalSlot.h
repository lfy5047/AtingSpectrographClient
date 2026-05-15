// 跨线程 signal-slot 框架（类似 Qt QueuedConnection）
// 支持：线程池异步分发、SlotTracker 生命周期追踪、自动断开、Signal 安全析构
#pragma once

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <tuple>
#include <vector>

namespace ss {

// ─── 线程池（全局单例）─────────────────────────────────────────────────────

class ThreadPool {
public:
    static ThreadPool& instance() {
        static ThreadPool pool(std::max(2u, std::thread::hardware_concurrency()));
        return pool;
    }

    template <typename F>
    void post(F&& f) {
        { std::lock_guard<std::mutex> lk(mtx_); q_.emplace(std::forward<F>(f)); }
        cv_.notify_one();
    }

    ~ThreadPool() {
        { std::lock_guard<std::mutex> lk(mtx_); stop_ = true; }
        cv_.notify_all();
        for (auto& t : workers_) if (t.joinable()) t.join();
    }

private:
    explicit ThreadPool(unsigned n) {
        for (unsigned i = 0; i < n; ++i)
            workers_.emplace_back([this] {
                for (;;) {
                    std::function<void()> task;
                    { std::unique_lock<std::mutex> lk(mtx_);
                      cv_.wait(lk, [this]{ return stop_ || !q_.empty(); });
                      if (stop_ && q_.empty()) return;
                      task = std::move(q_.front()); q_.pop(); }
                    task();
                }
            });
    }
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> q_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_ = false;
};

// ─── 连接句柄 ──────────────────────────────────────────────────────────────

using ConnId = uint64_t;

class Connection {
public:
    Connection() = default;
    Connection(ConnId id, std::function<void()> fn) : id_(id), fn_(std::move(fn)) {}
    void disconnect() { if (fn_) { fn_(); fn_ = nullptr; } }
    ConnId id() const { return id_; }
private:
    ConnId id_ = 0;
    std::function<void()> fn_;
};

// RAII 封装：析构时自动 disconnect
class ScopedConnection {
public:
    ScopedConnection() = default;
    explicit ScopedConnection(Connection c) : c_(std::move(c)) {}
    ~ScopedConnection() { c_.disconnect(); }
    ScopedConnection(ScopedConnection&& o) noexcept : c_(std::move(o.c_)) {}
    ScopedConnection& operator=(ScopedConnection&& o) noexcept {
        if (this != &o) { c_.disconnect(); c_ = std::move(o.c_); }
        return *this;
    }
    ScopedConnection(const ScopedConnection&) = delete;
    ScopedConnection& operator=(const ScopedConnection&) = delete;
    void disconnect() { c_.disconnect(); }
private:
    Connection c_;
};

// ─── 生命周期追踪基类 ──────────────────────────────────────────────────────
// 继承此类后，connect(this, …) 时：
//   1. 连接自动 track，对象析构时自动 disconnect
//   2. 对象析构后 Queued 调用自动跳过

class SlotTracker {
public:
    SlotTracker() : alive_(std::make_shared<bool>(true)) {}
    virtual ~SlotTracker() { *alive_ = false; }

    SlotTracker(const SlotTracker&) : alive_(std::make_shared<bool>(true)) {}
    SlotTracker& operator=(const SlotTracker&) { return *this; }

    std::weak_ptr<bool> guard() const { return alive_; }
    void track(ScopedConnection c) { conns_.push_back(std::move(c)); }

private:
    std::shared_ptr<bool> alive_;
    std::vector<ScopedConnection> conns_;
};

// ─── 连接类型 ──────────────────────────────────────────────────────────────

enum class ConnType {
    Direct,  // 同线程直接调用
    Queued   // 通过线程池异步分发
};

// ─── Signal ────────────────────────────────────────────────────────────────

template <typename... Args>
class Signal {
    struct Entry {
        ConnId id;
        std::function<void(Args...)> fn;
        std::weak_ptr<bool> guard;
        bool tracked;
        ConnType type;
        bool dead = false;
        bool ok() const { return !dead && (!tracked || !guard.expired()); }
    };

    // 内部状态独立于 Signal 对象存活，确保 Connection::disconnect() 安全
    struct Impl {
        std::mutex mtx;
        std::vector<Entry> slots;
        ConnId next_id = 1;
    };
    std::shared_ptr<Impl> impl_ = std::make_shared<Impl>();

    void gc() {
        auto& s = impl_->slots;
        s.erase(std::remove_if(s.begin(), s.end(),
            [](const Entry& e){ return !e.ok(); }), s.end());
    }

    Connection make_conn(ConnId id) {
        std::weak_ptr<Impl> w = impl_;
        return {id, [w, id] {
            if (auto p = w.lock()) {
                std::lock_guard<std::mutex> lk(p->mtx);
                for (auto& e : p->slots)
                    if (e.id == id) { e.dead = true; return; }
            }
        }};
    }

public:
    Signal() = default;
    ~Signal() {
        std::lock_guard<std::mutex> lk(impl_->mtx);
        impl_->slots.clear();
    }
    Signal(const Signal&) = delete;
    Signal& operator=(const Signal&) = delete;

    // 连接普通函数 / lambda（无生命周期追踪）
    Connection connect(std::function<void(Args...)> fn,
                       ConnType type = ConnType::Direct) {
        std::lock_guard<std::mutex> lk(impl_->mtx);
        auto id = impl_->next_id++;
        impl_->slots.push_back({id, std::move(fn), {}, false, type});
        return make_conn(id);
    }

    // 连接带 SlotTracker 的 slot：自动 track，析构时 disconnect；返回值可提前手动断开
    Connection connect(SlotTracker* trk,
                       std::function<void(Args...)> fn,
                       ConnType type = ConnType::Direct) {
        ConnId id;
        {
            std::lock_guard<std::mutex> lk(impl_->mtx);
            id = impl_->next_id++;
            impl_->slots.push_back({id, std::move(fn), trk->guard(), true, type});
        }
        trk->track(ScopedConnection(make_conn(id)));
        return make_conn(id);
    }

    void disconnect(ConnId id) {
        std::lock_guard<std::mutex> lk(impl_->mtx);
        for (auto& e : impl_->slots)
            if (e.id == id) { e.dead = true; return; }
    }

    void disconnect_all() {
        std::lock_guard<std::mutex> lk(impl_->mtx);
        impl_->slots.clear();
    }

    void emit(const Args&... args) {
        std::vector<Entry> snap;
        {
            std::lock_guard<std::mutex> lk(impl_->mtx);
            gc();
            snap = impl_->slots;
        }

        // 多个 Queued slot 共享同一份参数拷贝，避免重复分配
        std::shared_ptr<std::tuple<std::decay_t<Args>...>> shared;
        for (auto& e : snap) {
            if (!e.ok()) continue;

            if (e.type == ConnType::Direct) {
                e.fn(args...);
            } else {
                if (!shared)
                    shared = std::make_shared<std::tuple<std::decay_t<Args>...>>(args...);
                auto fn = e.fn;
                auto g  = e.guard;
                bool t  = e.tracked;
                auto a  = shared;
                ThreadPool::instance().post([fn, g, t, a] {
                    if (t) { auto p = g.lock(); if (!p || !*p) return; }
                    std::apply(fn, *a);
                });
            }
        }
    }

    void operator()(const Args&... args) { emit(args...); }
};

} // namespace ss
