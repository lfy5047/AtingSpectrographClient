#ifndef CIRCULAR_QUEUE_H
#define CIRCULAR_QUEUE_H

#include <cstddef>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

template <typename T>
class CircularQueue {
public:
    explicit CircularQueue(std::size_t capacity = 1)
        : buffer_(capacity), head_(0), tail_(0), size_(0) {
        if (capacity == 0) {
            throw std::invalid_argument("CircularQueue capacity must be greater than 0");
        }
    }

    // 入队，返回是否成功
    bool push(const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_[tail_] = value;
        if (size_ == capacityUnlocked()) {
            advanceHeadUnlocked();
        } else {
            ++size_;
        }
        advanceTailUnlocked();
        return true;
    }

    // 入队，返回是否成功
    bool push(T&& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_[tail_] = std::move(value);
        if (size_ == capacityUnlocked()) {
            advanceHeadUnlocked();
        } else {
            ++size_;
        }
        advanceTailUnlocked();
        return true;
    }

    // 原地构造入队，返回是否成功
    template <typename... Args>
    bool emplace(Args&&... args) {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_[tail_] = T(std::forward<Args>(args)...);
        if (size_ == capacityUnlocked()) {
            advanceHeadUnlocked();
        } else {
            ++size_;
        }
        advanceTailUnlocked();
        return true;
    }

    // 出队，返回是否成功
    bool pop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (size_ == 0) {
            return false;
        }
        advanceHeadUnlocked();
        --size_;
        return true;
    }

    // 出队，返回是否成功
    bool pop(T& out) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (size_ == 0) {
            return false;
        }
        out = std::move(buffer_[head_]);
        advanceHeadUnlocked();
        --size_;
        return true;
    }

    // 获取队首元素，返回是否成功
    bool front(T& out) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (size_ == 0) {
            return false;
        }
        out = buffer_[head_];
        return true;
    }

    // 获取队首元素
    T front() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (size_ == 0) {
            throw std::out_of_range("CircularQueue is empty");
        }
        return buffer_[head_];
    }

    // 获取队列大小
    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_;
    }

    // 获取队列容量
    std::size_t capacity() const {
        return capacityUnlocked();
    }

    // 判断队列是否为空
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_ == 0;
    }

    // 判断队列是否满
    bool full() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_ == capacityUnlocked();
    }

    // 清空队列
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        head_ = 0;
        tail_ = 0;
        size_ = 0;
    }

private:
    std::size_t capacityUnlocked() const {
        return buffer_.size();
    }

    void advanceHeadUnlocked() {
        head_ = (head_ + 1) % capacityUnlocked();
    }

    void advanceTailUnlocked() {
        tail_ = (tail_ + 1) % capacityUnlocked();
    }

    std::vector<T> buffer_;
    std::size_t head_;
    std::size_t tail_;
    std::size_t size_;
    mutable std::mutex mutex_;
};

#endif // CIRCULAR_QUEUE_H
