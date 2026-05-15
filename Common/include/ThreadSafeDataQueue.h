#ifndef THREAD_SAFE_DATA_QUEUE_H
#define THREAD_SAFE_DATA_QUEUE_H

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <queue>
#include <utility>

// 线程安全的有界数据队列，支持满时丢弃最旧元素、带超时阻塞消费。
template <typename T>
class ThreadSafeDataQueue {
public:
    // 非阻塞入队；当队列已达 capacity 时弹掉最旧元素再入队
    void pushDropOldest(T item, size_t capacity) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (queue_.size() >= capacity)
            queue_.pop();
        queue_.push(std::move(item));
        cv_.notify_one();
    }

    // 带超时的阻塞出队，running 为 false 时立即返回 false
    bool waitPopFor(T& out, int wait_ms, const std::atomic<bool>& running) {
        std::unique_lock<std::mutex> lk(mtx_);
        cv_.wait_for(lk, std::chrono::milliseconds(wait_ms),
                     [&] { return !queue_.empty() || !running; });
        if (queue_.empty()) return false;
        out = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    // 非阻塞出队，队列空时返回 false
    bool tryPop(T& out) {
        std::lock_guard<std::mutex> lk(mtx_);
        if (queue_.empty()) return false;
        out = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    void notifyAll() { cv_.notify_all(); }

    size_t size() const {
        std::lock_guard<std::mutex> lk(mtx_);
        return queue_.size();
    }

    void clear() {
        std::lock_guard<std::mutex> lk(mtx_);
        std::queue<T> empty;
        queue_.swap(empty);
    }

private:
    std::queue<T> queue_;
    mutable std::mutex mtx_;
    std::condition_variable cv_;
};

#endif // THREAD_SAFE_DATA_QUEUE_H
