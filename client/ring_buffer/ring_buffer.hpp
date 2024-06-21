#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <vector>
#include <stdexcept>

template <typename T>
class RingBuffer {
public:
    explicit RingBuffer(size_t size);
    
    void put(T item);
    T get();
    T get_nth_from_recent(size_t n); // New function
    
    bool is_empty() const;
    bool is_full() const;
    size_t size() const;

private:
    std::vector<T> buffer;
    size_t max_size;
    size_t head;
    size_t tail;
    bool full;
};

#include "ring_buffer.tpp"

#endif // RING_BUFFER_H
