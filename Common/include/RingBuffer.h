#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <algorithm>
#include <cstddef>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

template <typename T>
class RingBuffer {
public:
    explicit RingBuffer(std::size_t capacity)
        : buffer_(capacity), head_(0), tail_(0), size_(0), cursorOffset_(0) {
        if (capacity == 0) {
            throw std::invalid_argument("RingBuffer capacity must be greater than 0");
        }
    }

    bool write(const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        writeOneUnlocked(value);
        return true;
    }

    bool write(T&& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        writeOneUnlocked(std::move(value));
        return true;
    }

    template <typename... Args>
    bool emplace(Args&&... args) {
        std::lock_guard<std::mutex> lock(mutex_);
        writeOneUnlocked(T(std::forward<Args>(args)...));
        return true;
    }

    std::size_t write(const T* data, std::size_t count) {
        if (data == nullptr || count == 0) {
            return 0;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        for (std::size_t i = 0; i < count; ++i) {
            writeOneUnlocked(data[i]);
        }
        return count;
    }

    bool read(T& out) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (size_ == 0) {
            return false;
        }
        out = std::move(buffer_[head_]);
        onRemoveHeadUnlocked();
        advanceHeadUnlocked();
        --size_;
        return true;
    }

    std::size_t read(T* out, std::size_t count) {
        if (out == nullptr || count == 0) {
            return 0;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        const std::size_t n = std::min(count, size_);
        for (std::size_t i = 0; i < n; ++i) {
            out[i] = std::move(buffer_[head_]);
            onRemoveHeadUnlocked();
            advanceHeadUnlocked();
        }
        size_ -= n;
        return n;
    }

    bool peek(T& out) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (size_ == 0) {
            return false;
        }
        out = buffer_[head_];
        return true;
    }

    bool next(T& out) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (size_ == 0) {
            return false;
        }
        const std::size_t index = (head_ + cursorOffset_) % buffer_.size();
        out = buffer_[index];
        cursorOffset_ = (cursorOffset_ + 1) % size_;
        return true;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_;
    }

    std::size_t capacity() const {
        return buffer_.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_ == 0;
    }

    bool full() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return size_ == buffer_.size();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        head_ = 0;
        tail_ = 0;
        size_ = 0;
        cursorOffset_ = 0;
    }

private:
    template <typename V>
    void writeOneUnlocked(V&& value) {
        if (size_ == buffer_.size()) {
            onRemoveHeadUnlocked();
        }
        buffer_[tail_] = std::forward<V>(value);
        if (size_ == buffer_.size()) {
            advanceHeadUnlocked();
        } else {
            ++size_;
        }
        advanceTailUnlocked();
    }

    void onRemoveHeadUnlocked() {
        if (size_ == 0 || cursorOffset_ == 0) {
            return;
        }
        --cursorOffset_;
    }

    void advanceHeadUnlocked() {
        head_ = (head_ + 1) % buffer_.size();
    }

    void advanceTailUnlocked() {
        tail_ = (tail_ + 1) % buffer_.size();
    }

    std::vector<T> buffer_;
    std::size_t head_;
    std::size_t tail_;
    std::size_t size_;
    std::size_t cursorOffset_;
    mutable std::mutex mutex_;
};

#endif // RING_BUFFER_H
