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

struct Async;

class AsyncRef
{
    Async* ptr;
public:
    AsyncRef(Async* ptr = nullptr);
    AsyncRef(AsyncRef const& other) : AsyncRef{other.ptr} {}
    AsyncRef(AsyncRef&& other) : AsyncRef{} { swap(*this, other); }
    ~AsyncRef() { clear(); }

    auto clear() -> void;

    friend auto swap(AsyncRef& lhs, AsyncRef& rhs) noexcept -> void
    {
        using std::swap;
        static_assert(noexcept(lhs.ptr, rhs.ptr), "swap must be noexcept");
        swap(lhs.ptr, rhs.ptr);
    }

    auto operator=(AsyncRef other) -> AsyncRef&
    { swap(*this, other); return *this; }

    auto decay()        -> Async*       { return ptr; }
    auto decay() const  -> Async const* { return ptr; }
    auto get()          -> Async&       { return *ptr; }
    auto get() const    -> Async const& { return *ptr; }
    auto operator*()        -> Async&       { return *ptr; }
    auto operator*() const  -> Async const& { return *ptr; }
    auto operator->()       -> Async*       { return ptr; }
    auto operator->() const -> Async const* { return ptr; }
    operator bool() const { return ptr; }

    auto fold() -> AsyncRef&;
};

using Async_RawThunkCallback = Async* (*)(
        Destructible* context,
        Async* value);

struct Async_RawThunk
{
    Async_RawThunkCallback  callback;
    Destructible*           context;
    Async*                  dependency;
};

using Async_ThunkCallback = Async* (*)(
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
    AsyncRef left;
    AsyncRef right;
};

struct Async
{
    enum Async_Type type;
    size_t refcount;
    union {
        AsyncRef                        as_ptr;
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

    auto ptr_follow() -> Async&;

    auto has_category(Async_Type type) -> bool
    { return ptr_follow().type >= type; }

    auto has_type(Async_Type type) -> bool
    { return ptr_follow().type == type; }
};

//{{{ Ownership and Allocation: Async_new(), Async_ref(), Async_unref()

Async*
Async_new();

void
Async_ref(Async* self);

void
Async_unref(Async* self);

inline AsyncRef::AsyncRef(Async* ptr) : ptr{ptr} {
    if (ptr)
        Async_ref(ptr);
}

inline auto AsyncRef::clear() -> void {
    if (ptr)
        Async_unref(ptr);
    ptr = nullptr;
}

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
        AsyncRef& next,
        AsyncRef& blocked);

void
Async_unify(
        Async*  self,
        Async*  other);

inline Async*
Async_Ptr_follow(
        Async* self)
{ assert(self); return &self->ptr_follow(); }

inline auto AsyncRef::fold() -> AsyncRef&
{
    Async* target = &ptr->ptr_follow();
    if (target != ptr)
        *this = target;
    return *this;
}

inline static
AsyncRef&
Async_Ptr_fold(AsyncRef& ptr)
{
    return ptr.fold();
}

inline
bool
Async_has_type(
        Async* self,
        enum Async_Type type)
{ assert(self); return self->has_type(type); }

inline
bool
Async_has_category(
        Async* self,
        enum Async_Type category)
{ assert(self); return self->has_category(category); }

void
Async_run_until_completion(
        Async*  async);
