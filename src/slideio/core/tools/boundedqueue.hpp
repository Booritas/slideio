// This file is part of slideio project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://slideio.com/license.html.
//
#pragma once
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <functional>
#include <optional>
#include <map>

// --- Thread-safe bounded queue ---
template<typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(size_t maxSize) : maxSize_(maxSize) {}

    bool push(T item) {
        std::unique_lock lock(mutex_);
        cvNotFull_.wait(lock, [&] { return queue_.size() < maxSize_ || done_; });
        if (done_) 
            return false;  // Queue is shutting down, discard item
        queue_.push(std::move(item));
        cvNotEmpty_.notify_one();
        return true;
    }
    std::optional<T> pop() {
        std::unique_lock lock(mutex_);
        cvNotEmpty_.wait(lock, [&]{ return !queue_.empty() || done_; });
        if (queue_.empty()) 
            return std::nullopt;  // Signals shutdown
        T item = std::move(queue_.front());
        queue_.pop();
        cvNotFull_.notify_one();
        return item;
    }

    void setDone() {
        std::unique_lock lock(mutex_);
        done_ = true;
        cvNotEmpty_.notify_all();
        cvNotFull_.notify_all();
    }

private:
    std::queue<T>           queue_;
    std::mutex              mutex_;
    std::condition_variable cvNotEmpty_;
    std::condition_variable cvNotFull_;
    size_t                  maxSize_;
    bool                    done_ = false;
};