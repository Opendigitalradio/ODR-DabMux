#include <queue>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <tsduck.h>

class CircularBuffer {
public:
    void push(const ts::TSPacketVector& data) {
        std::unique_lock<std::mutex> lock(mutex_);
        buffer_.push(data);
        //printf("pushed, len: %ld\n", buffer_.size());
        lock.unlock();
        cond_.notify_one();     
    }

    ts::TSPacketVector pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        while (buffer_.empty()) {
            cond_.wait(lock);
        }
        ts::TSPacketVector data = buffer_.front();
        buffer_.pop();
        //printf("popped\n");
        return data;
    }

    size_t size() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return buffer_.size();
    }

private:
    std::queue<ts::TSPacketVector> buffer_;
    mutable std::mutex mutex_;
    std::condition_variable cond_;
};
