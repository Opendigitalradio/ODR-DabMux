#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <tsduck.h>

class PacketBuffer {
public:
    void push(const std::vector<uint8_t>&data) {
        std::unique_lock<std::mutex> lock(mutex_);
        buffer_.push(data);
        lock.unlock();
        cond_.notify_one();     
    }

    std::vector<uint8_t> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        while (buffer_.empty()) {
            cond_.wait(lock);
        }
        std::vector<uint8_t> data = buffer_.front();
        buffer_.pop();
        return data;
    }

    size_t size() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return buffer_.size();
    }

private:
    std::queue<std::vector<uint8_t>> buffer_;
    mutable std::mutex mutex_;
    std::condition_variable cond_;
};