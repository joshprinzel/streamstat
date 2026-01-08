#pragma once

#include <deque>
#include <mutex>
#include <condition_variable>

template <typename T>
class BoundedQueue {
private:
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::deque<T> queue_;
    const std::size_t capacity_;
    bool closed_{false};

public:
    explicit BoundedQueue(std::size_t max_size)
        : capacity_(max_size) {}

    /*
        size()

        Invariants:
        - Returns the current number of items in the queue
        - Protected by mutex (consistent snapshot)
        - O(1)
    */
    [[nodiscard]] std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    /*
        try_push (rvalue overload)

        Invariants:
        - Never blocks
        - Linearizable: item is either enqueued exactly once or not at all
        - Queue size never exceeds capacity_

        Behavior:
        - If queue is full:
            * return false
            * item is NOT enqueued
        - Else:
            * enqueue item (move)
            * notify one waiting consumer
            * return true

        System meaning:
        - Backpressure is enforced by dropping work at ingress
        - Producers never block
        - Queue memory usage is bounded
    */
    bool try_push(T&& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(closed_) return false;
        if (queue_.size() >= capacity_) return false;
        queue_.push_back(std::move(item));
        not_empty_.notify_one();
        return true;
    }

    /*
        try_push (lvalue overload)

        Semantics:
        - Copies item into the queue
        - Caller retains ownership of item
    */
    bool try_push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(closed_) return false;
        if (queue_.size() >= capacity_) return false;
        queue_.push_back(item);
        not_empty_.notify_one();
        return true;
    }

    /*
        pop()

        Invariants:
        - Blocks while queue is empty and not closed
        - Uses condition_variable to release mutex while waiting
        - Returns false iff (closed_ && queue is empty)

        Behavior:
        - Wait until:
            * queue has an item, OR
            * queue is closed
        - If queue has item:
            * pop exactly one item
            * move it into `out`
            * return true
        - Else (closed and empty):
            * return false
    */
    bool pop(T& out) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_.wait(lock, [&] {
            return closed_ || !queue_.empty();
        });

        if (queue_.empty()) return false;

        out = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }

    /*
        close()

        Invariants:
        - Idempotent
        - After close():
            * No new blocking pops will wait forever
            * Existing items may still be drained
            * pop() returns false once queue is empty

        System meaning:
        - Enables graceful shutdown of worker threads
    */
    void close() {
        std::lock_guard<std::mutex> lock(mutex_);
        closed_ = true;
        not_empty_.notify_all();
    }
};
