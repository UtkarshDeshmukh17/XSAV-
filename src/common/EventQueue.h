#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>
#include "Event.h"
#include "IEventSink.h"

namespace xsav {

// Generic thread-safe blocking queue.
// File Monitor's OS callback thread must never block on pipeline work
// (hashing, ML inference, etc.) or it will miss filesystem events /
// overflow the OS notification buffer. This queue is the hand-off point:
// the watch thread pushes, a separate worker thread pops and processes.
template <typename T>
class EventQueue {
public:
    void Push(T item) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(std::move(item));
        }
        cv_.notify_one();
    }

    // Blocks until an item is available or Shutdown() is called.
    // Returns std::nullopt only after Shutdown() and the queue has drained.
    std::optional<T> Pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !queue_.empty() || shuttingDown_; });
        if (queue_.empty()) {
            return std::nullopt;
        }
        T item = std::move(queue_.front());
        queue_.pop();
        return item;
    }

    void Shutdown() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            shuttingDown_ = true;
        }
        cv_.notify_all();
    }

    size_t Size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<T> queue_;
    bool shuttingDown_ = false;
};

// Adapts an EventQueue<FileEvent> as an IEventSink, so FileMonitor can
// be pointed straight at a queue without knowing a queue is involved.
// Whoever builds the Metadata Collection stage just needs to run a
// worker thread that calls queue.Pop() in a loop - see main.cpp for
// exactly where that consumer loop lives.
class QueueEventSink : public IEventSink {
public:
    explicit QueueEventSink(EventQueue<FileEvent>& queue) : queue_(queue) {}
    void OnFileEvent(const FileEvent& event) override {
        queue_.Push(event);
    }

private:
    EventQueue<FileEvent>& queue_;
};

} // namespace xsav
