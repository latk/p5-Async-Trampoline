#pragma once

#include "BasicDynamicArray.h"

#ifndef CIRCULAR_BUFFER_DEPENDENCY_MEMMOVE
#include <string.h>
#define CIRCULAR_BUFFER_DEPENDENCY_MEMMOVE(T, from, to, n)                  \
    memmove(to, from, sizeof(T) * n)
#endif

#include <utility>
#include <stdexcept>

template<class TValue>
class CircularBuffer
{
    BASIC_DYNAMIC_ARRAY(TValue) m_storage;
    size_t m_size;
    size_t m_start;

    size_t map_index(size_t i) const { return (m_start + i) % capacity(); }

    size_t next_capacity() const
    {
        size_t newcap = 2 * capacity();
        if (newcap == 0) newcap = 1;
        return newcap;
    }

public:

    CircularBuffer() :
        m_storage(BASIC_DYNAMIC_ARRAY_INIT),
        m_size(0),
        m_start(0)
    {}

    ~CircularBuffer()
    {
        while (size())
            deq();
        BASIC_DYNAMIC_ARRAY_FREE(m_storage);
        m_size = 0;
        m_start = 0;
    }

    /** Number of enqueued elements.
     */
    size_t size() const { return m_size; }

    /** Allocated size.
     */
    size_t capacity() const { return m_storage.size; }

    size_t _internal_start() const { return m_start; }

    /** Increase the capacity.
     *
     *  newcapacity: size
     *      Must be larger than current capacity().
     *      Should be power-of-2.
     */
    void grow(size_t newcapacity)
    {
        size_t tail_length = capacity() - m_start;
        if (m_size < tail_length)
            tail_length = m_size;
        bool ok = false;
        BASIC_DYNAMIC_ARRAY_GROW(ok, TValue, m_storage, newcapacity);
        if (!ok)
            throw std::runtime_error("could not grow CircularBuffer storage");

        // [345_012] --GROW-> [345_012_______] --> [345________012]
        if (ok && m_start > 0 && tail_length > 0)
        {
            TValue* data = m_storage.data;
            TValue* it = &data[m_start];
            TValue* end = it + tail_length;
            TValue* target = &data[m_start = capacity() - tail_length];
            while (it != end) *target++ = std::move(*it++);
        }
    }

    /** Enqueue a value, growing the buffer if necessary.
     *
     *  value: TValue
     */
    template<class T>
    void enq(T&& value)
    {
        if (size() == capacity())
        {
            size_t newcapacity = next_capacity();
            grow(newcapacity);
        }

        size_t i = map_index(size());
        m_storage.data[i] = std::forward<T>(value);
        m_size++;
    }

    /** Enqueue a value, growing the buffer if necessary.
     *
     *  provider: () -> TValue
     *      invoked to create the value.
     *      Only called when `ok` is true.
     */
    template<class TProvider>
    void enq_from_cb(TProvider provider)
    {
        if (size() == capacity())
        {
            size_t newcapacity = next_capacity();
            grow(newcapacity);
        }

        size_t i = map_index(size());
        m_storage.data[i] = provider();
        m_size++;
    }

    /** Dequeue the oldest value.
     *
     *  Precondition:
     *      size() != 0
     */
    TValue deq()
    {
        assert(size());

        TValue val = std::move(m_storage.data[m_start]);

        m_size--;
        m_start = map_index(1);

        return val;
    }

    /** Dequeue the newest value.
     *
     *  Precondition:
     *      size() != 0
     */
    TValue deq_back()
    {
        assert(size());

        m_size--;

        size_t i = map_index(m_size);

        return std::move(m_storage.data[i]);
    }
};

