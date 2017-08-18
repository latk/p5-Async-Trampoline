#pragma once

#include "BasicDynamicArray.h"

#ifndef CIRCULAR_BUFFER_DEPENDENCY_MEMMOVE
#include <string.h>
#define CIRCULAR_BUFFER_DEPENDENCY_MEMMOVE(T, from, to, n)                  \
    memmove(to, from, sizeof(T) * n)
#endif

#define CIRCULAR_BUFFER(TValue)                                             \
    struct {                                                                \
        BASIC_DYNAMIC_ARRAY(TValue) storage;                                \
        size_t size;                                                        \
        size_t start;                                                       \
    }

#define CIRCULAR_BUFFER_INIT \
    { BASIC_DYNAMIC_ARRAY_INIT, 0, 0 }

#define CIRCULAR_BUFFER_FREE(buffer) do {                                   \
    BASIC_DYNAMIC_ARRAY_FREE((buffer).storage);                             \
    (buffer).size = 0;                                                      \
    (buffer).start = 0;                                                     \
} while (0)

#define CIRCULAR_BUFFER_GROW(ok, TValue, buffer, capacity) do {             \
    size_t _circular_buffer_start = (buffer).start;                         \
    size_t _circular_buffer_tail_length =                                   \
        (buffer).storage.size - _circular_buffer_start;                     \
    if ((buffer).size < _circular_buffer_tail_length)                       \
        _circular_buffer_tail_length = (buffer).size;                       \
    BASIC_DYNAMIC_ARRAY_GROW(ok, TValue, (buffer).storage, capacity);       \
    /* [345_012] --GROW-> [345_012_______] --> [345________012] */          \
    if (ok  && (_circular_buffer_start > 0)                                 \
            && (_circular_buffer_tail_length > 0)) {                        \
        TValue* _circular_buffer_data = (buffer).storage.data;              \
        TValue* _circular_buffer_it = &_circular_buffer_data[               \
            _circular_buffer_start];                                        \
        TValue* _circular_buffer_target = &_circular_buffer_data[           \
            (buffer).start =                                                \
                (buffer).storage.size - _circular_buffer_tail_length];      \
        CIRCULAR_BUFFER_DEPENDENCY_MEMMOVE(                                 \
                TValue,                                                     \
                _circular_buffer_it,                                        \
                _circular_buffer_target,                                    \
                _circular_buffer_tail_length);                              \
    }                                                                       \
} while (0)

#define CIRCULAR_BUFFER_SIZE(buffer)                                        \
    ((buffer).size)

#define CIRCULAR_BUFFER_ENQ(ok, TValue, buffer, value) do {                 \
    (ok) = 1;                                                               \
    if ((buffer).size == (buffer).storage.size) {                           \
        size_t _circular_buffer_newsize = 2 * (buffer).size;                \
        if (_circular_buffer_newsize == 0) _circular_buffer_newsize = 1;    \
        CIRCULAR_BUFFER_GROW(                                               \
                ok, TValue, (buffer), _circular_buffer_newsize);            \
    }                                                                       \
    if (ok) {                                                               \
        size_t _circular_buffer_i =                                         \
            CIRCULAR_BUFFER_MAP_INDEX(buffer, (buffer).size);               \
        (buffer).storage.data[_circular_buffer_i] = (value);                \
        (buffer).size++;                                                    \
    }                                                                       \
} while (0)

// precondition: buffer size != 0
#define CIRCULAR_BUFFER_DEQ(var, buffer) do {                               \
    (var) = (buffer).storage.data[(buffer).start];                          \
    (buffer).size--;                                                        \
    (buffer).start = CIRCULAR_BUFFER_MAP_INDEX(buffer, 1);                  \
} while (0)

#define CIRCULAR_BUFFER_MAP_INDEX(buffer, i)                                \
    (((buffer).start + (i)) % (buffer).storage.size)
