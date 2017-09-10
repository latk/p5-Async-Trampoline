#include "Async.h"

#define UNUSED(x) (void)(x)

#define BINARY_INIT(name, type)                                             \
    void Async_ ## name ## _init(Async* self, Async* left, Async* right)    \
    { binary_init(self, type, left, right); }

#define BINARY_INIT_MOVE(name, type)                                        \
    void Async_ ## name ## _init_move(Async* self, Async* other)            \
    { binary_init_move(self, type, other); }

#define BINARY_CLEAR(name, type)                                            \
    void Async_ ## name ## _clear(Async* self)                              \
    { binary_clear(self, type); }

#define BINARY_INIT_MOVE_CLEAR(name, type)                                  \
    BINARY_INIT(name, type)                                                 \
    BINARY_INIT_MOVE(name, type)                                            \
    BINARY_CLEAR(name, type)

#define ASSERT_INIT(self) do {                                              \
    assert(self);                                                           \
    assert(self->type == Async_Type::IS_UNINITIALIZED);                           \
} while (0)

#define ASSERT_INIT_MOVE(self, other, other_type) do {                      \
    ASSERT_INIT(self);                                                      \
    assert(other);                                                          \
    assert(other->type == other_type);                                      \
} while (0)

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

void
Async_unify(
        Async*  self,
        Async*  other)
{
    assert(self);
    assert(other);

    ASYNC_LOG_DEBUG("unify %p with %p\n", self, other);

    Async* unref_me = NULL;

    if (self->type != Async_Type::IS_UNINITIALIZED)
    {
        Async_ref(unref_me = other);  // in case "other" was owned by "self"
        Async_clear(self);
    }

    if (Async_has_type(other, Async_Type::IS_CANCEL))
    {
        Async_Cancel_init(self);
    }
    else if (other->refcount > 1)
    {
        Async_Ptr_init(self, other);
    }
    else
    {

        switch (other->type)
        {
            case Async_Type::IS_UNINITIALIZED:
                break;

            case Async_Type::CATEGORY_INITIALIZED:
                assert(0);
                break;
            case Async_Type::IS_PTR:
                Async_Ptr_init_move(
                        self, other);  // TODO really?
                break;
            case Async_Type::IS_RAWTHUNK:
                Async_RawThunk_init_move(
                        self, other);
                break;
            case Async_Type::IS_THUNK:
                Async_Thunk_init_move(
                        self, other);
                break;
            case Async_Type::IS_CONCAT:
                Async_Concat_init_move(
                        self, other);
                break;
            case Async_Type::IS_COMPLETE_THEN:
                Async_CompleteThen_init_move(
                        self, other);
                break;
            case Async_Type::IS_RESOLVED_OR:
                Async_ResolvedOr_init_move(
                        self, other);
                break;
            case Async_Type::IS_RESOLVED_THEN:
                Async_ResolvedThen_init_move(
                        self, other);
                break;
            case Async_Type::IS_VALUE_OR:
                Async_ValueOr_init_move(
                        self, other);
                break;
            case Async_Type::IS_VALUE_THEN:
                Async_ValueThen_init_move(
                        self, other);
                break;

            case Async_Type::CATEGORY_COMPLETE:
                assert(0);
                break;
            case Async_Type::IS_CANCEL:
                Async_Cancel_init_move(
                        self, other);
                break;

            case Async_Type::CATEGORY_RESOLVED:
                assert(0);
                break;
            case Async_Type::IS_ERROR:
                Async_Error_init_move(
                        self, other);
                break;
            case Async_Type::IS_VALUE:
                Async_Value_init_move(
                        self, other);
                break;

            default:
                assert(0);
        }
    }

    if (unref_me)
        Async_unref(unref_me);

    return;
}

// Binary

static
void
binary_init(
        Async*          self,
        enum Async_Type type,
        Async*          left,
        Async*          right)
{
    ASSERT_INIT(self);
    assert(left);
    assert(right);

    self->type = type;
    self->as_binary.left = &left->ptr_follow();
    self->as_binary.right = &right->ptr_follow();
}

static
void
binary_init_move(
        Async*          self,
        enum Async_Type type,
        Async*          other)
{
    ASSERT_INIT_MOVE(self, other, type);

    self->type = type;
    other->type = Async_Type::IS_UNINITIALIZED;

    using std::swap;
    swap(self->as_binary, other->as_binary);
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
    self->as_binary.left.clear();
    self->as_binary.right.clear();
}

// Ptr

void
Async_Ptr_init(
        Async*  self,
        Async*  target)
{
    ASSERT_INIT(self);
    assert(target);

    self->type = Async_Type::IS_PTR;
    self->as_ptr = &target->ptr_follow();
}

void
Async_Ptr_init_move(
        Async*  self,
        Async*  other)
{
    ASSERT_INIT_MOVE(self, other, Async_Type::IS_PTR);

    self->type = Async_Type::IS_PTR;
    other->type = Async_Type::IS_UNINITIALIZED;

    using std::swap;
    swap(self->as_ptr, other->as_ptr);
}

void
Async_Ptr_clear(
        Async*  self)
{
    assert(self);
    assert(self->type == Async_Type::IS_PTR);

    self->type = Async_Type::IS_UNINITIALIZED;
    self->as_ptr.clear();
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
Async_RawThunk_init_move(
        Async*  self,
        Async*  other)
{
    ASSERT_INIT_MOVE(self, other, Async_Type::IS_RAWTHUNK);

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
        Async*              dependency)
{
    ASYNC_LOG_DEBUG(
            "init Async %p to Thunk: callback=%p context.data=%p dependency=%p\n",
            self, callback, context.data, dependency);

    ASSERT_INIT(self);
    assert(callback);
    assert(context.vtable);

    if (dependency)
        Async_ref(dependency = Async_Ptr_follow(dependency));

    self->type = Async_Type::IS_THUNK;
    self->as_thunk.dependency = dependency;
    self->as_thunk.callback = callback;
    using std::swap;
    swap(self->as_thunk.context, context);
}

void
Async_Thunk_init_move(
        Async*  self,
        Async*  other)
{
    ASSERT_INIT_MOVE(self, other, Async_Type::IS_THUNK);

    using std::swap;

    self->type = Async_Type::IS_THUNK;
    other->type = Async_Type::IS_UNINITIALIZED;

    swap(self->as_thunk.dependency, other->as_thunk.dependency);

    swap(self->as_thunk.callback, other->as_thunk.callback);

    swap(self->as_thunk.context, other->as_thunk.context);
}

void
Async_Thunk_clear(
        Async*  self)
{
    assert(self);
    assert(self->type == Async_Type::IS_THUNK);

    Async* dependency = self->as_thunk.dependency;
    if (dependency)
        Async_unref(dependency);

    self->type = Async_Type::IS_UNINITIALIZED;
    self->as_thunk.dependency = NULL;
    self->as_thunk.callback = NULL;
    self->as_thunk.context.clear();
}

BINARY_INIT_MOVE_CLEAR(Concat,          Async_Type::IS_CONCAT)
BINARY_INIT_MOVE_CLEAR(CompleteThen,    Async_Type::IS_COMPLETE_THEN)
BINARY_INIT_MOVE_CLEAR(ResolvedOr,      Async_Type::IS_RESOLVED_OR)
BINARY_INIT_MOVE_CLEAR(ResolvedThen,    Async_Type::IS_RESOLVED_THEN)
BINARY_INIT_MOVE_CLEAR(ValueOr,         Async_Type::IS_VALUE_OR)
BINARY_INIT_MOVE_CLEAR(ValueThen,       Async_Type::IS_VALUE_THEN)

// Cancel

void
Async_Cancel_init(
        Async* self)
{
    ASSERT_INIT(self);

    self->type = Async_Type::IS_CANCEL;
}

void
Async_Cancel_init_move(
        Async*  self,
        Async*  other)
{
    ASSERT_INIT_MOVE(self, other, Async_Type::IS_CANCEL);

    Async_Cancel_init(self);
    Async_Cancel_clear(other);
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
    self->as_error = error;
}

void
Async_Error_init_move(
        Async*  self,
        Async*  other)
{
    ASSERT_INIT_MOVE(self, other, Async_Type::IS_ERROR);

    self->type = Async_Type::IS_ERROR;
    other->type = Async_Type::IS_UNINITIALIZED;

    using std::swap;
    swap(self->as_error, other->as_error);
}

void
Async_Error_clear(
        Async*  self)
{
    assert(self);
    assert(self->type == Async_Type::IS_ERROR);

    self->type = Async_Type::IS_UNINITIALIZED;
    self->as_error.clear();
}

// Value

void
Async_Value_init(
        Async*                          self,
        DestructibleTuple*              values)
{
    ASSERT_INIT(self);

    if (ASYNC_TRAMPOLINE_DEBUG)
    {
        size_t size = (values) ? values->size : 0;
        ASYNC_LOG_DEBUG(
                "init Async %p to Values: values = %p { size = %zu }\n",
                self, values, size);
        for (size_t i = 0; i < size; i++)
            ASYNC_LOG_DEBUG("  - values %p\n",
                    values->at(i));
    }

    self->type = Async_Type::IS_VALUE;
    self->as_value = values;
}

void
Async_Value_init_move(
        Async*  self,
        Async*  other)
{
    ASSERT_INIT_MOVE(self, other, Async_Type::IS_VALUE);
    assert(other->refcount == 1);

    self->type = Async_Type::IS_VALUE;
    self->as_value = other->as_value;
    other->as_value = NULL;

    Async_Value_clear(other);
}

void
Async_Value_clear(
        Async*  self)
{
    assert(self);
    assert(self->type == Async_Type::IS_VALUE);

    self->type = Async_Type::IS_UNINITIALIZED;

    if (self->as_value)
        self->as_value->clear();
}
