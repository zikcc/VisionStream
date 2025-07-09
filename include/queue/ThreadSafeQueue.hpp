#pragma once
#include <condition_variable>
#include <mutex>
#include <queue>

template <typename T>
class ThreadSafeQueue {
   public:
    void push(const T& value) {
        std::lock_guard<std::mutex> lock(mtx_);
        queue_.push(value);
        cv_.notify_one();
    }

    bool pop(T& value) {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this] { return !queue_.empty(); });
        value = queue_.front();
        queue_.pop();
        return true;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mtx_);
        std::queue<T> empty;
        std::swap(queue_, empty);
    }

   private:
    std::queue<T> queue_;
    std::mutex mtx_;
    std::condition_variable cv_;
};
