#pragma once
#include "DynamicArray.h"
#include "Destructible.h"

#include <stddef.h>
#include <assert.h>
#include <stdbool.h>

#ifndef ASYNC_TRAMPOLINE_DEBUG
#define ASYNC_TRAMPOLINE_DEBUG 0
#define ASYNC_LOG_DEBUG(...) do {} while (0)
#else
#include <stdio.h>
#define ASYNC_TRAMPOLINE_DEBUG 1
#define ASYNC_LOG_DEBUG(...) do {                                         \
    fprintf(stderr, "#DEBUG Async: " __VA_ARGS__);           \
    fflush(stderr);                                                         \
} while (0)
#endif /* ifndef ASYNC_TRAMPOLINE_DEBUG */

enum class Async_Type
{
    IS_UNINITIALIZED,

    CATEGORY_INITIALIZED,
    IS_PTR,
    IS_RAWTHUNK,
    IS_THUNK,
    IS_CONCAT,
    IS_COMPLETE_THEN,
    IS_RESOLVED_OR,
    IS_RESOLVED_THEN,
    IS_VALUE_OR,
    IS_VALUE_THEN,

    CATEGORY_COMPLETE,
    IS_CANCEL,

    CATEGORY_RESOLVED,
    IS_ERROR,
    IS_VALUE,
};

inline
const char*
Async_Type_name(enum Async_Type type)
{
    switch (type) {
        case Async_Type::IS_UNINITIALIZED:      return "IS_UNINITIALIZED";
        case Async_Type::CATEGORY_INITIALIZED:  return "CATEGORY_INITIALIZED";
        case Async_Type::IS_PTR:                return "IS_PTR";
        case Async_Type::IS_RAWTHUNK:           return "IS_RAWTHUNK";
        case Async_Type::IS_THUNK:              return "IS_THUNK";
        case Async_Type::IS_CONCAT:             return "IS_CONCAT";
        case Async_Type::IS_COMPLETE_THEN:      return "IS_COMPLETE_THEN";
        case Async_Type::IS_RESOLVED_OR:        return "IS_RESOLVED_OR";
        case Async_Type::IS_RESOLVED_THEN:      return "IS_RESOLVED_THEN";
        case Async_Type::IS_VALUE_OR:           return "IS_VALUE_OR";
        case Async_Type::IS_VALUE_THEN:         return "IS_VALUE_THEN";
        case Async_Type::CATEGORY_COMPLETE:     return "CATEGORY_COMPLETE";
        case Async_Type::IS_CANCEL:             return "IS_CANCEL";
        case Async_Type::CATEGORY_RESOLVED:     return "CATEGORY_RESOLVED";
        case Async_Type::IS_ERROR:              return "IS_ERROR";
        case Async_Type::IS_VALUE:              return "IS_VALUE";
        default:                                return "(unknown)";
    }
}

typedef struct Async Async;

typedef Async* (*Async_RawThunkCallback)(
        Destructible* context,
        Async* value);

struct Async_RawThunk
{
    Async_RawThunkCallback  callback;
    Destructible*           context;
    Async*                  dependency;
};

typedef Async* (*Async_ThunkCallback)(
        Destructible*       context,
        DestructibleTuple*  data);

struct Async_Thunk
{
    Async_ThunkCallback callback;
    Destructible        context;
    Async*              dependency;
};

struct Async_Pair
{
    Async* left;
    Async* right;
};

struct Async
{
    enum Async_Type type;
    size_t refcount;
    union {
        Async*                          as_ptr;
        struct Async_Thunk              as_thunk;
        struct Async_Pair               as_binary;
        Destructible                    as_error;
        DestructibleTuple*              as_value;
    };

    Async() :
        type{Async_Type::IS_UNINITIALIZED},
        refcount{1},
        as_ptr{nullptr}
    { }
    ~Async();
};

//{{{ Ownership and Allocation: Async_new(), Async_ref(), Async_unref()

Async*
Async_new();

void
Async_ref(Async* self);

void
Async_unref(Async* self);

//}}}

//{{{ Initialization: Async_X_init()
//
// IS_UNINITIALIZED -> IS_X

void
Async_Ptr_init(
        Async*  self,
        Async*  target);

void
Async_RawThunk_init(
        Async*                  self,
        Async_RawThunkCallback  callback,
        Destructible            context,
        Async*                  dependency);

void
Async_Thunk_init(
        Async*              self,
        Async_ThunkCallback callback,
        Destructible        context,
        Async*              dependency);

void
Async_Concat_init(
        Async*  self,
        Async*  left,
        Async*  right);

void
Async_CompleteThen_init(
        Async*  self,
        Async*  first,
        Async*  then);

void
Async_ResolvedOr_init(
        Async*  self,
        Async*  first,
        Async*  orelse);

void
Async_ResolvedThen_init(
        Async*  self,
        Async*  first,
        Async*  then);

void
Async_ValueOr_init(
        Async*  self,
        Async*  first,
        Async*  orelse);

void
Async_ValueThen_init(
        Async*  self,
        Async*  first,
        Async*  then);

void
Async_Cancel_init(
        Async*  self);

void
Async_Error_init(
        Async*              self,
        Destructible        error);

void
Async_Value_init(
        Async*                          self,
        DestructibleTuple*              values);

//}}}

//{{{ Move initialization: Async_X_init_move()
//
// (IS_UNINITIALIZED, IS_X) -> (IS_X, IS_UNINITIALIZED)

void
Async_Ptr_init_move(
        Async* self,
        Async* other);

void
Async_RawThunk_init_move(
        Async* self,
        Async* other);

void
Async_Thunk_init_move(
        Async* self,
        Async* other);

void
Async_Concat_init_move(
        Async* self,
        Async* other);

void
Async_CompleteThen_init_move(
        Async* self,
        Async* other);

void
Async_ResolvedOr_init_move(
        Async* self,
        Async* other);

void
Async_ResolvedThen_init_move(
        Async* self,
        Async* other);

void
Async_ValueOr_init_move(
        Async* self,
        Async* other);

void
Async_ValueThen_init_move(
        Async* self,
        Async* other);

void
Async_Cancel_init_move(
        Async* self,
        Async* other);

void
Async_Error_init_move(
        Async* self,
        Async* other);

void
Async_Value_init_move(
        Async* self,
        Async* other);

//}}}

//{{{ Clearing: Async_X_clear()
//
// IS_X -> IS_UNINITIALIZED

void
Async_clear(
        Async* self);

void
Async_Ptr_clear(
        Async* self);

void
Async_RawThunk_clear(
        Async* self);

void
Async_Thunk_clear(
        Async* self);

void
Async_Concat_clear(
        Async* self);

void
Async_CompleteThen_clear(
        Async* self);

void
Async_ResolvedOr_clear(
        Async* self);

void
Async_ResolvedThen_clear(
        Async* self);

void
Async_ValueOr_clear(
        Async* self);

void
Async_ValueThen_clear(
        Async* self);

void
Async_Cancel_clear(
        Async* self);

void
Async_Error_clear(
        Async* self);

void
Async_Value_clear(
        Async* self);

inline Async::~Async() { Async_clear(this); }

//}}}

// Evaluation: Async_X_evaluate()
// Incomplete -> Complete
void
Async_eval(
        Async*  self,
        Async** next,
        Async** blocked);

void
Async_unify(
        Async*  self,
        Async*  other);

Async*
Async_Ptr_follow(
        Async* self);

inline static
Async*
Async_Ptr_fold(Async** ptr)
{
    Async* target = Async_Ptr_follow(*ptr);
    if (target != *ptr)
    {
        Async_ref(target);
        Async_unref(*ptr);
        *ptr = target;
    }
    return *ptr;
}

bool
Async_has_type(
        Async* self,
        enum Async_Type type);

bool
Async_has_category(
        Async* self,
        enum Async_Type category);

void
Async_run_until_completion(
        Async*  async);
