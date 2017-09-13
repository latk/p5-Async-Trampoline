#include "Async.h"

#include <memory>
#include <utility>

#define UNUSED(x) (void)(x)

#define BINARY_INIT(name, type)                                             \
    void Async::set_to_ ## name (AsyncRef left, AsyncRef right)             \
    { set_to_Binary(*this, type, std::move(left), std::move(right)); }

#define ASSERT_INIT(self) do {                                              \
    assert((self)->type == Async_Type::IS_UNINITIALIZED);                   \
} while (0)

static void set_to_Binary(Async& self, Async_Type, AsyncRef, AsyncRef);

static void Async_Ptr_clear        (Async* self);
static void Async_RawThunk_clear   (Async* self);
static void Async_Thunk_clear      (Async* self);
static void Async_Binary_clear     (Async& self, Async_Type type);
static void Async_Flow_clear       (Async& self);
static void Async_Cancel_clear     (Async* self);
static void Async_Error_clear      (Async* self);
static void Async_Value_clear      (Async* self);

// Polymorphic

auto Async::clear() -> void
{
    switch (type) {
        case Async_Type::IS_UNINITIALIZED:
            break;

        case Async_Type::CATEGORY_INITIALIZED:
            assert(0);
            break;
        case Async_Type::IS_PTR:
            Async_Ptr_clear(this);
            break;
        case Async_Type::IS_RAWTHUNK:
            Async_RawThunk_clear(this);
            break;
        case Async_Type::IS_THUNK:
            Async_Thunk_clear(this);
            break;
        case Async_Type::IS_CONCAT:
            Async_Binary_clear(*this, type);
            break;
        case Async_Type::IS_FLOW:
            Async_Flow_clear(*this);
            break;

        case Async_Type::CATEGORY_COMPLETE:
            assert(0);
            break;
        case Async_Type::IS_CANCEL:
            Async_Cancel_clear(this);
            break;

        case Async_Type::CATEGORY_RESOLVED:
            assert(0);
            break;
        case Async_Type::IS_ERROR:
            Async_Error_clear(this);
            break;
        case Async_Type::IS_VALUE:
            Async_Value_clear(this);
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
            set_to_Ptr(std::move(other.as_ptr));
            Async_Ptr_clear(&other);
            break;
        case Async_Type::IS_RAWTHUNK:
            assert(0); // TODO not implemented
            break;
        case Async_Type::IS_THUNK:
            set_to_Thunk(
                    std::move(other.as_thunk.callback),
                    std::move(other.as_thunk.dependency));
            Async_Thunk_clear(&other);
            break;
        case Async_Type::IS_CONCAT:
            set_to_Binary(
                    *this,
                    other.type,
                    std::move(other.as_binary.left),
                    std::move(other.as_binary.right));
            Async_Binary_clear(other, other.type);
            break;
        case Async_Type::IS_FLOW:
            set_to_Flow(std::move(other.as_flow));
            Async_Flow_clear(other);

        case Async_Type::CATEGORY_COMPLETE:
            assert(0);
            break;
        case Async_Type::IS_CANCEL:
            set_to_Cancel();
            Async_Cancel_clear(&other);
            break;

        case Async_Type::CATEGORY_RESOLVED:
            assert(0);
            break;
        case Async_Type::IS_ERROR:
            set_to_Error(std::move(other.as_error));
            Async_Error_clear(&other);
            break;
        case Async_Type::IS_VALUE:
            set_to_Value(std::move(other.as_value));
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

void Async::set_to_Ptr(
        AsyncRef target)
{
    ASSERT_INIT(this);
    assert(target);

    target.fold();

    type = Async_Type::IS_PTR;
    new (&as_ptr) AsyncRef { std::move(target) };
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

void Async::set_to_RawThunk(
        Async_RawThunk::Callback    callback,
        AsyncRef                    dependency)
{
    ASSERT_INIT(this);
    assert(callback);

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

void Async::set_to_Thunk(
        Async_Thunk::Callback   callback,
        AsyncRef                dependency)
{
    ASYNC_LOG_DEBUG(
            "init Async %p to Thunk: callback=??? dependency=%p\n",
            this, dependency.decay());

    ASSERT_INIT(this);
    assert(callback);

    if (dependency)
        dependency.fold();

    type = Async_Type::IS_THUNK;
    new (&as_thunk) Async_Thunk{
        std::move(callback),
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

// Binary

static void set_to_Binary(
        Async&      self,
        Async_Type  type,
        AsyncRef    left,
        AsyncRef    right)
{
    ASSERT_INIT(&self);
    assert(left);
    assert(right);

    left.fold();
    right.fold();

    self.type = type;
    new (&self.as_binary) Async_Pair {
        std::move(left),
        std::move(right),
    };
}

static void Async_Binary_clear(
        Async&      self,
        Async_Type  type)
{
    assert(self.type == type);

    self.type = Async_Type::IS_UNINITIALIZED;
    self.as_binary.~Async_Pair();
}

BINARY_INIT(Concat,     Async_Type::IS_CONCAT)

// Flow

void Async::set_to_Flow(Async_Flow flow)
{
    ASSERT_INIT(this);
    assert(flow.left);
    assert(flow.right);

    flow.left.fold();
    flow.right.fold();

    type = Async_Type::IS_FLOW;
    new (&as_flow) Async_Flow { std::move(flow) };
}

static void Async_Flow_clear(Async& self)
{
    assert(self.type == Async_Type::IS_FLOW);

    self.type = Async_Type::IS_UNINITIALIZED;
    self.as_flow.~Async_Flow();
}

// Cancel

void Async::set_to_Cancel()
{
    ASSERT_INIT(this);

    type = Async_Type::IS_CANCEL;
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

void Async::set_to_Error(
        Destructible    error)
{
    ASSERT_INIT(this);
    assert(error.vtable);

    type = Async_Type::IS_ERROR;
    new (&as_error) Destructible { std::move(error) };
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

void Async::set_to_Value(
        DestructibleTuple               values)
{
    ASSERT_INIT(this);

    if (ASYNC_TRAMPOLINE_DEBUG)
    {
        ASYNC_LOG_DEBUG(
                "init Async %p to Values: values = %p { size = %zu }\n",
                this, values.data.get(), values.size);
        for (auto val : values)
            ASYNC_LOG_DEBUG("  - values %p\n", val);
    }

    type = Async_Type::IS_VALUE;
    new (&as_value) DestructibleTuple{std::move(values)};
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
