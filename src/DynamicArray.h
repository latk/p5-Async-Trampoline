#pragma once

/* == dependencies == */

#ifndef DYNAMIC_ARRAY_DEPENDENCY_REALLOC
#include <stdlib.h>
#define DYNAMIC_ARRAY_DEPENDENCY_REALLOC(orig, TValue, n) \
    realloc(orig, sizeof(TValue) * n)
#endif /* ifndef DYNAMIC_ARRAY_DEPENDENCY_REALLOC(orig, TValue, n) */

#ifndef DYNAMIC_ARRAY_DEPENDENCY_MZERO
#include <string.h>
#define DYNAMIC_ARRAY_DEPENDENCY_MZERO(ptr, TValue, n) \
    memset(ptr, 0, sizeof(TValue) * n)
#endif /* ifndef DYNAMIC_ARRAY_DEPENDENCY_MZERO(ptr, TValue, n) */

#ifndef DYNAMIC_ARRAY_DEPENDENCY_FREE
#include <stdlib.h>
#define DYNAMIC_ARRAY_DEPENDENCY_FREE(ptr) \
    free(ptr)
#endif /* ifndef DYNAMIC_ARRAY_DEPENDENCY_FREE(ptr) */

/* == BasicDynamicArray dependencies == */

#ifndef BASIC_DYNAMIC_ARRAY_DEPENDENCY_REALLOC
#define BASIC_DYNAMIC_ARRAY_DEPENDENCY_REALLOC(ptr, TValue, n) \
    DYNAMIC_ARRAY_DEPENDENCY_REALLOC(ptr, TValue, n)
#endif /* ifndef BASIC_DYNAMIC_ARRAY_DEPENDENCY_REALLOC(ptr, TValue, n) */

#ifndef BASIC_DYNAMIC_ARRAY_DEPENDENCY_FREE
#define BASIC_DYNAMIC_ARRAY_DEPENDENCY_FREE(ptr) \
    DYNAMIC_ARRAY_DEPENDENCY_FREE(ptr)
#endif /* ifndef BASIC_DYNAMIC_ARRAY_DEPENDENCY_FREE(ptr) */

#include "BasicDynamicArray.h"

/** Declare a dynamic array type.
 *
 *      DYNAMIC_ARRAY(TValue)
 *
 *  TValue: TYPE
 *      the value type of the dynamic array.
 *
 *  returns: TYPE LITERAL
 *      an unnamed type literal.
 *
 *  Example:
 *
 *      typedef DYNAMIC_ARRAY(int) MyIntArray;
 */
#define DYNAMIC_ARRAY(TValue) \
    struct { \
        BASIC_DYNAMIC_ARRAY(TValue) base; \
        size_t size; \
    }

/** Release all resources held by this dynamic array.
 *
 *      void DYNAMIC_ARRAY_FREE(array)
 *
 *  If the array elements hold resources,
 *  you must free them manually first.
 *
 *  array: dynamic array
 *      The array to be freed.
 *      May be evaluated multiple times.
 */
#define DYNAMIC_ARRAY_FREE(array) \
    do { \
        BASIC_DYNAMIC_ARRAY_FREE((array).base); \
        (array).size = 0; \
    } while (0)

/** Move the array contents from source to target.
 *
 *      void DYNAMIC_ARRAY_MOVE(target, source)
 *
 *  The elements themselves will not be moved or reallocated.
 *  Pointers are not invalidated.
 *  This "steals" the allocated buffer.
 *
 *  target: dynamic array
 *      The array that should receive the elements.
 *      Should be empty prior to this call,
 *      see DYNAMIC_ARRAY_FREE().
 *  source: dynamic array
 *      The array providing the elements.
 *      After this call, it will be empty.
 */
#define DYNAMIC_ARRAY_MOVE(target, source) do {                             \
    BASIC_DYNAMIC_ARRAY_MOVE((target).base, (source).base);                 \
    (target).size = (source).size;                                          \
    (source).size = 0;                                                      \
} while (0)

/** Grow the array to a specific capacity.
 *
 *      void DYNAMIC_ARRAY_RESERVE(ok, TValyue, array, capacity)
 *
 *  If the requested capacity is smaller than the current capacity,
 *  no allocation is performed.
 *
 *  ok: bool LVALUE
 *      a variable that will be assigned
 *      a true value if the reservation succeeded, or
 *      a false value if reservation failed.
 *      This should be checked after each call.
 *      May be evaluated multiple times.
 *
*  TValue: TYPE
 *      the value type of the dynamic array.
 *
 *  array: dynamic array
 *      The dynamic array to resize.
 *      May be evaluated multiple times.
 *
 *  capacity: unsigned
 *      The new requested capacity.
 *      The capacity counts the number of elements, not the size in bytes.
 *      May be evaluated multiple times.
 *
 *  Example:
 *
 *      bool ok;
 *      DYNAMIC_ARRAY_RESERVE(ok, int, myarray, 8);
 *      if (!ok) {
 *          ... // malloc failed
 *      }
 */
#define DYNAMIC_ARRAY_RESERVE(ok, TValue, array, capacity) \
    do { \
        size_t _dynamic_array_oldcap = (array).base.size; \
        BASIC_DYNAMIC_ARRAY_GROW(ok, TValue, (array).base, capacity); \
        if (ok && _dynamic_array_oldcap < capacity) { \
            DYNAMIC_ARRAY_DEPENDENCY_MZERO(\
                    (array).base.data + _dynamic_array_oldcap, \
                    TValue, \
                    (capacity) - _dynamic_array_oldcap); \
        } \
    } while (0)

/** Grow the array and initialize empty elements
 *
 *      void DYNAMIC_ARRAY_EXTEND(ok, TValue, array, size, expr)
 *
 *  ok: bool LVALUE
 *      true on success,
 *      false if a reallocation failed.
 *      May be evaluated multiple times.
 *
 *  TValue: TYPE
 *      The value type.
 *
 *  array: dynamic array
 *      The array to extend.
 *      May be evaluated multiple times.
 *
 *  size: size_t
 *      The new size of the array.
 *
 *  expr: TValue EXPRESSION
 *      An expression used to initialize new elements.
 *      Guaranteed to be evaluated once for each uninitialized element.
 *
 *  Pseudocode:
 *
 *      book ok = 1;
 *      DYNAMIC_ARRAY_RESERVE(ok, TValue, array, size);
 *      while (ok && DYNAMIC_ARRAY_SIZE(array) < size)
 *          DYNAMIC_ARRAY_PUSH(ok, TValue, array, expr);
 *
 *  Example:
 *
 *      bool ok;
 *      int i =  0;
 *      DYNAMIC_ARRAY_EXTEND(ok, int, my_array, 8, (i++));
 *
 */
#define DYNAMIC_ARRAY_EXTEND(ok, TValue, array, size_, expr) \
    do { \
        BASIC_DYNAMIC_ARRAY_GROW(ok, TValue, (array).base, size_); \
        if (ok) { \
            for (size_t _dynamic_array_i = (array).size \
                    ; _dynamic_array_i < (size_) \
                    ; _dynamic_array_i++) { \
                (array).base.data[_dynamic_array_i] = (expr); \
            } \
        } \
    } while (0)

/** Initialize an empty dynamic array.
 *
 *      DYNAMIC_ARRAY_INIT
 *
 *  returns: LITERAL
 *      a dynamic array literal.
 *
 *  Example:
 *
 *      MyIntArray xs = DYNAMIC_ARRAY_INIT;
 *
 */
#define DYNAMIC_ARRAY_INIT { BASIC_DYNAMIC_ARRAY_INIT, 0 }

/** Get the number of stored elements in the array.
 *
 *      size_t DYNAMIC_ARRAY_SIZE(array)
 *
 *  array: dynamic array
 *      The array for which the size is requested,
 *      Guaranteed to be evaluated only once.
 *
 *  returns: size_t
 *      The number of elements currently stored in the array.
 *
 *  Example:
 *
 *      printf("array has %d elements\n", DYNAMIC_ARRAY_SIZE(get_array()));
 */
#define DYNAMIC_ARRAY_SIZE(array) ((array).size)

/** Get a pointer to some element.
 *
 *      TValue* DYNAMIC_ARRAY_GETPTR(array, i)
 *
 *  array: dynamic array
 *      The array in which the element should be accessed.
 *      May be evaluated multiple times.
 *
 *  i: size_t
 *      The index of the element.
 *      May be evaluated multiple times.
 *
 *  returns: TValue*
 *      a pointer to that element if an element exists at that index,
 *      or a NULL pointer if the index is out of bounds.
 *
 *  Example: get an element
 *
 *      int* maybe_x = DYNAMIC_ARRAY_GETPTR(my_array, 4);
 *      if (maybe_x != NULL)
 *          printf("my_array[4] = %d", *maybe_x);
 *
 *  Example: set an element
 *
 *      int* maybe_x = DYNAMIC_ARRAY_GETPTR(my_array, 4);
 *      if (maybe_x != NULL)
 *          *maybe_x = 42;
 *
 */
#define DYNAMIC_ARRAY_GETPTR(array, i) \
    ((0 <= (i) && (i) < (array).size) ? &(array).base.data[i] : NULL)

/** Push one element onto the end of the array, resizing if necessary.
 *
 *      void DYNAMIC_ARRAY_PUSH(ok, TValue, array, value)
 *
 *  ok: bool LVALUE
 *      a variable that is assigned a true value or success,
 *      or a false value if a reallocation failed.
 *      May be evaluated multiple times.
 *
 *  TValue: TYPE
 *      The value type of the array.
 *
 *  array: dynamic array
 *      The array that will receive the value.
 *      May be evaluated multiple times.
 *
 *  value: TValue
 *      The value to push onto the array.
 *      Guaranteed to be evaluated at most once.
 *
 *  Example:
 *
 *      bool ok;
 *      DYNAMIC_ARRAY_PUSH(ok, int, my_array, 42);
 *      if (!ok)
 *          return;  // realloc failed
 */
#define DYNAMIC_ARRAY_PUSH(ok, TValue, array, value) \
    do { \
        (ok) = 1; \
        if ((array).size == (array).base.size) \
            DYNAMIC_ARRAY_RESERVE((ok), TValue, (array), \
                    ((array).base.size) ? 2 * (array).base.size : 1); \
        if (ok) \
            (array).base.data[(array).size++] = (value); \
    } while (0)

/** Remove the last element from the array.
 *
 *      void DYNAMIC_ARRAY_POP(var, TValue, array)
 *
 *  Precondition:
 *      The array contains at least one element.
  *
 *  var: TValue LVALUE
 *      the removed element will be assigned to this variable.
 *      Guaranteed to be evaluated at most once.
 *
 *  TValue: TYPE
 *      The value type.
 *
 *  array: dynamic array
 *      The array from which the last element should be removed.
 *
 *  Example:
 *
 *      if (DYNAMIC_ARRAY_SIZE(my_array) == 0)
 *          return;
 *
 *      int x;
 *      DYNAMIC_ARRAY_POP(x, int, my_array);
 *      printf("pop(my_array) = %d\n", x);
 */
#define DYNAMIC_ARRAY_POP(var, TValue, array) \
    do { \
        size_t _dynamic_array_i = --(array).size; \
        (var) = (array).base.data[_dynamic_array_i]; \
        DYNAMIC_ARRAY_DEPENDENCY_MZERO(&(array).base.data[_dynamic_array_i], TValue, 1); \
    } while (0)
