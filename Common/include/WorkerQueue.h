#ifndef WORKER_QUEUE_H
#define WORKER_QUEUE_H

#include <atomic>
#include <cstddef>
#include <thread>
#include <utility>
#include "ThreadSafeDataQueue.h"

// 工作线程 + 生产者-消费者队列基类。
// 子类必须实现 processOne(T)；如需在 worker 退出前做收尾（如排空、刷盘），重写 onWorkerExit()。
//
// 注意：子类析构必须先调用 stop()，否则 worker 可能调到已析构子类的 processOne。
template <typename T>
class WorkerQueue {
public:
    struct WorkerConfig {
        size_t queue_capacity = 256;
        int    wait_ms        = 50;
    };

    explicit WorkerQueue(WorkerConfig cfg = {}) : cfg_(cfg) {}
    virtual ~WorkerQueue() { stop(); }

    WorkerQueue(const WorkerQueue&) = delete;
    WorkerQueue& operator=(const WorkerQueue&) = delete;

    void start() {
        if (running_) return;
        running_ = true;
        worker_ = std::thread(&WorkerQueue::workerLoop, this);
    }

    void stop() {
        if (!running_) return;
        running_ = false;
        queue_.notifyAll();
        if (!worker_.joinable()) return;
        if (std::this_thread::get_id() == worker_.get_id()) {
            worker_.detach();
            return;
        }
        worker_.join();
    }

    // 非阻塞入队；满时丢弃最旧元素
    virtual void push(T item) {
        queue_.pushDropOldest(std::move(item), cfg_.queue_capacity);
    }

protected:
    virtual void processOne(T item) = 0;
    virtual void onWorkerExit() {}

    // 供 onWorkerExit 排空残留元素使用
    bool tryPop(T& out) { return queue_.tryPop(out); }

    size_t queueSize() const { return queue_.size(); }
    void queueClear() { queue_.clear(); }

private:
    void workerLoop() {
        while (running_) {
            T item;
            if (queue_.waitPopFor(item, cfg_.wait_ms, running_))
                processOne(std::move(item));
        }
        onWorkerExit();
    }

    WorkerConfig cfg_;
    ThreadSafeDataQueue<T> queue_;
    std::atomic<bool> running_{false};
    std::thread worker_;
};

#endif // WORKER_QUEUE_H
