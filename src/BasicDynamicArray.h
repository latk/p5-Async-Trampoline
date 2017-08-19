#pragma once

#ifndef BASIC_DYNAMIC_ARRAY_DEPENDENCY_REALLOC
#include <stdlib.h>
#define BASIC_DYNAMIC_ARRAY_DEPENDENCY_REALLOC(ptr, TValue, n) \
    realloc(ptr, sizeof(TValue) * n)
#endif /* ifndef BASIC_DYNAMIC_ARRAY_DEPENDENCY_REALLOC(ptr, TValue, n) */

#ifndef BASIC_DYNAMIC_ARRAY_DEPENDENCY_FREE
#include <stdlib.h>
#define BASIC_DYNAMIC_ARRAY_DEPENDENCY_FREE(ptr) \
    free(ptr);
#endif /* ifndef BASIC_DYNAMIC_ARRAY_DEPENDENCY_FREE(ptr) */

#define BASIC_DYNAMIC_ARRAY(TValue)         \
    struct {                                \
        TValue* data;                       \
        size_t size;                        \
    }

#define BASIC_DYNAMIC_ARRAY_INIT \
    { NULL, 0 }

#define BASIC_DYNAMIC_ARRAY_FREE(array) \
    do { \
        BASIC_DYNAMIC_ARRAY_DEPENDENCY_FREE((array).data); \
        (array).data = NULL; \
        (array).size = 0; \
    } while (0)

#define BASIC_DYNAMIC_ARRAY_GROW(ok, TValue, array, n) \
    do { \
        (ok) = 0; \
        if ((n) <= (array).size) { \
            (ok) = 1; \
        } else { \
            TValue* _basic_dynamic_array_ptr = \
                BASIC_DYNAMIC_ARRAY_DEPENDENCY_REALLOC((array).data, TValue, n); \
            if (_basic_dynamic_array_ptr != NULL) { \
                (array).data = _basic_dynamic_array_ptr; \
                (array).size = (n); \
                (ok) = 1; \
            } \
        } \
    } while (0)
