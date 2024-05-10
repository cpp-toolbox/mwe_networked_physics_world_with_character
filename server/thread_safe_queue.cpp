#include "thread_safe_queue.hpp"

template <typename T>
void ThreadSafeQueue<T>::push(const T& item) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(item);
    }
    cond_var_.notify_one();
}

template <typename T>
T ThreadSafeQueue<T>::pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    cond_var_.wait(lock, [this] { return !queue_.empty(); });

    T item = queue_.front();
    queue_.pop();
    return item;
}

template <typename T>
T ThreadSafeQueue<T>::front() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (queue_.empty()) {
        throw std::runtime_error("Queue is empty");
    }
    return queue_.front();
}

template <typename T>
bool ThreadSafeQueue<T>::empty() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.empty();
}

template <typename T>
size_t ThreadSafeQueue<T>::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}
