#include "Async.h"

#include <memory>
#include <utility>

#define UNUSED(x) (void)(x)

#define BINARY_INIT(name, type)                                             \
    void Async_ ## name ## _init(Async* self, AsyncRef left, AsyncRef right)\
    { binary_init(self, type, std::move(left), std::move(right)); }

#define BINARY_CLEAR(name, type)                                            \
    void Async_ ## name ## _clear(Async* self)                              \
    { binary_clear(self, type); }

#define BINARY_INIT_CLEAR(name, type)                                       \
    BINARY_INIT(name, type)                                                 \
    BINARY_CLEAR(name, type)

#define ASSERT_INIT(self) do {                                              \
    assert(self);                                                           \
    assert((self)->type == Async_Type::IS_UNINITIALIZED);                   \
} while (0)

// Binary

static
void
binary_init(
        Async*          self,
        enum Async_Type type,
        AsyncRef        left,
        AsyncRef        right)
{
    ASSERT_INIT(self);
    assert(left);
    assert(right);

    left.fold();
    right.fold();

    self->type = type;
    new (&self->as_binary) Async_Pair {
        std::move(left),
        std::move(right),
    };
}

static
void
binary_clear(
        Async*          self,
        enum Async_Type type)
{
    assert(self);
    assert(self->type == type);

    self->type = Async_Type::IS_UNINITIALIZED;
    self->as_binary.~Async_Pair();
}

// Polymorphic

void
Async_clear(
        Async* self)
{
    switch (self->type) {
        case Async_Type::IS_UNINITIALIZED:
            break;

        case Async_Type::CATEGORY_INITIALIZED:
            assert(0);
            break;
        case Async_Type::IS_PTR:
            Async_Ptr_clear(self);
            break;
        case Async_Type::IS_RAWTHUNK:
            Async_RawThunk_clear(self);
            break;
        case Async_Type::IS_THUNK:
            Async_Thunk_clear(self);
            break;
        case Async_Type::IS_CONCAT:
            Async_Concat_clear(self);
            break;
        case Async_Type::IS_COMPLETE_THEN:
            Async_CompleteThen_clear(self);
            break;
        case Async_Type::IS_RESOLVED_OR:
            Async_ResolvedOr_clear(self);
            break;
        case Async_Type::IS_RESOLVED_THEN:
            Async_ResolvedThen_clear(self);
            break;
        case Async_Type::IS_VALUE_OR:
            Async_ValueOr_clear(self);
            break;
        case Async_Type::IS_VALUE_THEN:
            Async_ValueThen_clear(self);
            break;

        case Async_Type::CATEGORY_COMPLETE:
            assert(0);
            break;
        case Async_Type::IS_CANCEL:
            Async_Cancel_clear(self);
            break;

        case Async_Type::CATEGORY_RESOLVED:
            assert(0);
            break;
        case Async_Type::IS_ERROR:
            Async_Error_clear(self);
            break;
        case Async_Type::IS_VALUE:
            Async_Value_clear(self);
            break;

        default:
            assert(0);
    }
}

auto Async::set_from(Async&& other) -> void
{
    assert(type == Async_Type::IS_UNINITIALIZED);

    switch (other.type)
    {
        case Async_Type::IS_UNINITIALIZED:
            break;

        case Async_Type::CATEGORY_INITIALIZED:
            assert(0);
            break;
        case Async_Type::IS_PTR:
            Async_Ptr_init(this, std::move(other.as_ptr));
            Async_Ptr_clear(&other);
            break;
        case Async_Type::IS_RAWTHUNK:
            assert(0); // TODO not implemented
            break;
        case Async_Type::IS_THUNK:
            Async_Thunk_init(this,
                    std::move(other.as_thunk.callback),
                    std::move(other.as_thunk.context),
                    std::move(other.as_thunk.dependency));
            Async_Thunk_clear(&other);
            break;
        case Async_Type::IS_CONCAT:
        case Async_Type::IS_COMPLETE_THEN:
        case Async_Type::IS_RESOLVED_OR:
        case Async_Type::IS_RESOLVED_THEN:
        case Async_Type::IS_VALUE_OR:
        case Async_Type::IS_VALUE_THEN:
            binary_init(
                    this,
                    other.type,
                    std::move(other.as_binary.left),
                    std::move(other.as_binary.right));
            binary_clear(&other, other.type);
            break;

        case Async_Type::CATEGORY_COMPLETE:
            assert(0);
            break;
        case Async_Type::IS_CANCEL:
            Async_Cancel_init(this);
            Async_Cancel_clear(&other);
            break;

        case Async_Type::CATEGORY_RESOLVED:
            assert(0);
            break;
        case Async_Type::IS_ERROR:
            Async_Error_init(this, std::move(other.as_error));
            Async_Error_clear(&other);
            break;
        case Async_Type::IS_VALUE:
            Async_Value_init(this, std::move(other.as_value));
            Async_Value_clear(&other);
            break;

        default:
            assert(0);
    }

}

auto Async::operator=(Async& other) -> Async&
{
    ASYNC_LOG_DEBUG("unify %p with %p\n", this, &other);

    // save other in case we might own it
    AsyncRef unref_me{};
    if (type != Async_Type::IS_UNINITIALIZED)
        unref_me = &other;

    clear();

    set_from(other.move_if_only_ref_else_ptr());

    return *this;
}

// Ptr

void
Async_Ptr_init(
        Async*      self,
        AsyncRef    target)
{
    ASSERT_INIT(self);
    assert(target);

    target.fold();

    self->type = Async_Type::IS_PTR;
    new (&self->as_ptr) AsyncRef { std::move(target) };
}

void
Async_Ptr_clear(
        Async*  self)
{
    assert(self);
    assert(self->type == Async_Type::IS_PTR);

    self->type = Async_Type::IS_UNINITIALIZED;
    self->as_ptr.~AsyncRef();
}

// RawThunk

void
Async_RawThunk_init(
        Async*                  self,
        Async_RawThunkCallback  callback,
        Destructible            context,
        Async*                  dependency)
{
    ASSERT_INIT(self);
    assert(callback);
    assert(context.vtable);

    UNUSED(dependency);

    assert(0); // TODO not implemented
}

void
Async_RawThunk_clear(
        Async*  self)
{
    assert(self);
    assert(self->type == Async_Type::IS_RAWTHUNK);

    assert(0);  // TODO not implemented
}

// Thunk

void
Async_Thunk_init(
        Async*              self,
        Async_ThunkCallback callback,
        Destructible        context,
        AsyncRef            dependency)
{
    ASYNC_LOG_DEBUG(
            "init Async %p to Thunk: callback=%p context.data=%p dependency=%p\n",
            self, callback, context.data, dependency.decay());

    ASSERT_INIT(self);
    assert(callback);
    assert(context.vtable);

    if (dependency)
        dependency.fold();

    self->type = Async_Type::IS_THUNK;
    new (&self->as_thunk) Async_Thunk{
        callback,
        std::move(context),
        std::move(dependency),
    };
}

void
Async_Thunk_clear(
        Async*  self)
{
    assert(self);
    assert(self->type == Async_Type::IS_THUNK);

    self->type = Async_Type::IS_UNINITIALIZED;
    self->as_thunk.~Async_Thunk();
}

BINARY_INIT_CLEAR(Concat,          Async_Type::IS_CONCAT)
BINARY_INIT_CLEAR(CompleteThen,    Async_Type::IS_COMPLETE_THEN)
BINARY_INIT_CLEAR(ResolvedOr,      Async_Type::IS_RESOLVED_OR)
BINARY_INIT_CLEAR(ResolvedThen,    Async_Type::IS_RESOLVED_THEN)
BINARY_INIT_CLEAR(ValueOr,         Async_Type::IS_VALUE_OR)
BINARY_INIT_CLEAR(ValueThen,       Async_Type::IS_VALUE_THEN)

// Cancel

void
Async_Cancel_init(
        Async* self)
{
    ASSERT_INIT(self);

    self->type = Async_Type::IS_CANCEL;
}

void
Async_Cancel_clear(
        Async* self)
{
    assert(self);
    assert(self->type == Async_Type::IS_CANCEL);

    self->type = Async_Type::IS_UNINITIALIZED;
}

// Error

void
Async_Error_init(
        Async*          self,
        Destructible    error)
{
    ASSERT_INIT(self);
    assert(error.vtable);

    self->type = Async_Type::IS_ERROR;
    new (&self->as_error) Destructible { std::move(error) };
}

void
Async_Error_clear(
        Async*  self)
{
    assert(self);
    assert(self->type == Async_Type::IS_ERROR);

    self->type = Async_Type::IS_UNINITIALIZED;
    self->as_error.~Destructible();
}

// Value

void
Async_Value_init(
        Async*                          self,
        DestructibleTuple               values)
{
    ASSERT_INIT(self);

    if (ASYNC_TRAMPOLINE_DEBUG)
    {
        ASYNC_LOG_DEBUG(
                "init Async %p to Values: values = %p { size = %zu }\n",
                self, values.data.get(), values.size);
        for (auto val : values)
            ASYNC_LOG_DEBUG("  - values %p\n", val);
    }

    self->type = Async_Type::IS_VALUE;
    new (&self->as_value) DestructibleTuple{std::move(values)};
}

void
Async_Value_clear(
        Async*  self)
{
    assert(self);
    assert(self->type == Async_Type::IS_VALUE);

    self->type = Async_Type::IS_UNINITIALIZED;

    self->as_value.~DestructibleTuple();
}
