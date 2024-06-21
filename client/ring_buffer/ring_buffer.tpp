#include "ring_buffer.hpp"

template <typename T>
RingBuffer<T>::RingBuffer(size_t size) : buffer(size), max_size(size), head(0), tail(0), full(false) {}

template <typename T>
void RingBuffer<T>::put(T item) {
    buffer[head] = item;
    if (full) {
        tail = (tail + 1) % max_size;
    }
    head = (head + 1) % max_size;
    full = head == tail;
}

template <typename T>
T RingBuffer<T>::get() {
    if (is_empty()) {
        throw std::runtime_error("Buffer is empty");
    }
    T item = buffer[tail];
    full = false;
    tail = (tail + 1) % max_size;
    return item;
}

template <typename T>
T RingBuffer<T>::get_nth_from_recent(size_t n) {
    if (n >= size()) {
        throw std::out_of_range("Index out of range");
    }
    size_t index = (head - 1 - n + max_size) % max_size;
    return buffer[index];
}

template <typename T>
bool RingBuffer<T>::is_empty() const {
    return (!full && (head == tail));
}

template <typename T>
bool RingBuffer<T>::is_full() const {
    return full;
}

template <typename T>
size_t RingBuffer<T>::size() const {
    size_t size = max_size;

    if (!full) {
        if (head >= tail) {
            size = head - tail;
        } else {
            size = max_size + head - tail;
        }
    }

    return size;
}
