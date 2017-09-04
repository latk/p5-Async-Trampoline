#include "Async.h"
#define UNUSED(x) (void)(x)

// Polymorphic

void
Async_clear(
        Async* self)
{
    switch (self->type) {
        case Async_IS_UNINITIALIZED:
            break;

        case Async_CATEGORY_INITIALIZED:
            assert(0);
            break;
        case Async_IS_PTR:
            Async_Ptr_clear(self);
            break;
        case Async_IS_RAWTHUNK:
            Async_RawThunk_clear(self);
            break;
        case Async_IS_THUNK:
            Async_Thunk_clear(self);
            break;
        case Async_IS_CONCAT:
            Async_Concat_clear(self);
            break;
        case Async_IS_COMPLETE_THEN:
            Async_CompleteThen_clear(self);
            break;
        case Async_IS_RESOLVED_OR:
            Async_ResolvedOr_clear(self);
            break;
        case Async_IS_RESOLVED_THEN:
            Async_ResolvedThen_clear(self);
            break;
        case Async_IS_VALUE_OR:
            Async_ValueOr_clear(self);
            break;
        case Async_IS_VALUE_THEN:
            Async_ValueThen_clear(self);
            break;

        case Async_CATEGORY_COMPLETE:
            assert(0);
            break;
        case Async_IS_CANCEL:
            Async_Cancel_clear(self);
            break;

        case Async_CATEGORY_RESOLVED:
            assert(0);
            break;
        case Async_IS_ERROR:
            Async_Error_clear(self);
            break;
        case Async_IS_VALUE:
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

    ASYNC_LOG_DEBUG("unify %p with %p", self, other);

    Async* unref_me = NULL;

    if (self->type != Async_IS_UNINITIALIZED)
    {
        Async_ref(unref_me = other);  // in case "other" was owned by "self"
        Async_clear(self);
    }

    if (Async_has_type(other, Async_IS_CANCEL))
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
            case Async_IS_UNINITIALIZED:
                break;

            case Async_CATEGORY_INITIALIZED:
                assert(0);
                break;
            case Async_IS_PTR:
                Async_Ptr_init_move(
                        self, other);  // TODO really?
                break;
            case Async_IS_RAWTHUNK:
                Async_RawThunk_init_move(
                        self, other);
                break;
            case Async_IS_THUNK:
                Async_Thunk_init_move(
                        self, other);
                break;
            case Async_IS_CONCAT:
                Async_Concat_init_move(
                        self, other);
                break;
            case Async_IS_COMPLETE_THEN:
                Async_CompleteThen_init_move(
                        self, other);
                break;
            case Async_IS_RESOLVED_OR:
                Async_ResolvedOr_init_move(
                        self, other);
                break;
            case Async_IS_RESOLVED_THEN:
                Async_ResolvedThen_init_move(
                        self, other);
                break;
            case Async_IS_VALUE_OR:
                Async_ValueOr_init_move(
                        self, other);
                break;
            case Async_IS_VALUE_THEN:
                Async_ValueThen_init_move(
                        self, other);
                break;

            case Async_CATEGORY_COMPLETE:
                assert(0);
                break;
            case Async_IS_CANCEL:
                Async_Cancel_init_move(
                        self, other);
                break;

            case Async_CATEGORY_RESOLVED:
                assert(0);
                break;
            case Async_IS_ERROR:
                Async_Error_init_move(
                        self, other);
                break;
            case Async_IS_VALUE:
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
    assert(self);
    assert(self->type == Async_IS_UNINITIALIZED);
    assert(left);
    assert(right);

    Async_ref(left);
    Async_ref(right);

    self->type = type;
    self->as_binary.left = left;
    self->as_binary.right = right;
}

static
void
binary_init_move(
        Async*          self,
        enum Async_Type type,
        Async*          other)
{
    assert(self);
    assert(self->type == Async_IS_UNINITIALIZED);
    assert(other);
    assert(other->type == type);

    self->type = type;
    other->type = Async_IS_UNINITIALIZED;

    self->as_binary = other->as_binary;
    other->as_binary.left = NULL;
    other->as_binary.right = NULL;
}

static
void
binary_clear(
        Async*          self,
        enum Async_Type type)
{
    assert(self);
    assert(self->type = type);

    Async* left = self->as_binary.left;
    Async* right = self->as_binary.right;

    self->type = Async_IS_UNINITIALIZED;
    self->as_binary.left = NULL;
    self->as_binary.right = NULL;

    Async_unref(left);
    Async_unref(right);
}

// Ptr

void
Async_Ptr_init(
        Async*  self,
        Async*  target)
{
    assert(self);
    assert(self->type == Async_IS_UNINITIALIZED);
    assert(target);

    target = Async_Ptr_follow(target);

    Async_ref(target);

    self->type = Async_IS_PTR;
    self->as_ptr = target;
}

void
Async_Ptr_init_move(
        Async*  self,
        Async*  other)
{
    assert(self);
    assert(self->type == Async_IS_UNINITIALIZED);
    assert(other);
    assert(other->type == Async_IS_PTR);

    self->type = Async_IS_PTR;
    other->type = Async_IS_UNINITIALIZED;

    self->as_ptr = other->as_ptr;
    other->as_ptr = NULL;
}

void
Async_Ptr_clear(
        Async*  self)
{
    assert(self);
    assert(self->type == Async_IS_PTR);

    Async_unref(self->as_ptr);

    self->type = Async_IS_UNINITIALIZED;
    self->as_ptr = NULL;
}

// RawThunk

void
Async_RawThunk_init(
        Async*                  self,
        Async_RawThunkCallback  callback,
        Destructible            context,
        Async*                  dependency)
{
    assert(self);
    assert(self->type == Async_IS_UNINITIALIZED);
    assert(callback);
    assert(context.vtable);

    assert(0); // TODO not implemented
}

void
Async_RawThunk_init_move(
        Async*  self,
        Async*  other)
{
    assert(self);
    assert(self->type == Async_IS_UNINITIALIZED);
    assert(other);
    assert(other->type == Async_IS_RAWTHUNK);

    assert(0); // TODO not implemented
}

void
Async_RawThunk_clear(
        Async*  self)
{
    assert(self);
    assert(self->type == Async_IS_RAWTHUNK);

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
            "init Async %p to Thunk: callback=%p context.data=%p dependency=%p",
            self, callback, context.data, dependency);

    assert(self);
    assert(self->type == Async_IS_UNINITIALIZED);
    assert(callback);

    if (dependency)
        Async_ref(dependency);

    self->type = Async_IS_THUNK;
    self->as_thunk.dependency = dependency;
    self->as_thunk.callback = callback;
    Destructible_init_move(
            &self->as_thunk.context, &context);
}

void
Async_Thunk_init_move(
        Async*  self,
        Async*  other)
{
    assert(self);
    assert(self->type == Async_IS_UNINITIALIZED);
    assert(other);
    assert(other->type == Async_IS_THUNK);

    self->type = Async_IS_THUNK;
    other->type = Async_IS_UNINITIALIZED;

    self->as_thunk.dependency = other->as_thunk.dependency;
    other->as_thunk.dependency = NULL;

    self->as_thunk.callback = other->as_thunk.callback;
    other->as_thunk.callback = NULL;

    Destructible_init_move(&self->as_thunk.context, &other->as_thunk.context);
}

void
Async_Thunk_clear(
        Async*  self)
{
    assert(self);
    assert(self->type == Async_IS_THUNK);

    Async* dependency = self->as_thunk.dependency;
    if (dependency)
        Async_unref(dependency);

    self->type = Async_IS_UNINITIALIZED;
    self->as_thunk.dependency = NULL;
    self->as_thunk.callback = NULL;
    Destructible_clear(&self->as_thunk.context);
}

// Concat

void
Async_Concat_init(
        Async*  self,
        Async*  left,
        Async*  right)
{
    binary_init(self, Async_IS_CONCAT, left, right);
}

void
Async_Concat_init_move(
        Async*  self,
        Async*  other)
{
    binary_init_move(self, Async_IS_CONCAT, other);
}

void
Async_Concat_clear(
        Async*  self)
{
    binary_clear(self, Async_IS_CONCAT);
}

// CompleteThen

void
Async_CompleteThen_init(
        Async*  self,
        Async*  first,
        Async*  then)
{
    binary_init(self, Async_IS_COMPLETE_THEN, first, then);
}

void
Async_CompleteThen_init_move(
        Async*  self,
        Async*  other)
{
    binary_init_move(self, Async_IS_COMPLETE_THEN, other);
}

void
Async_CompleteThen_clear(
        Async*  self)
{
    binary_clear(self, Async_IS_COMPLETE_THEN);
}

// ResolvedOr

void
Async_ResolvedOr_init(
        Async*  self,
        Async*  first,
        Async*  orelse)
{
    binary_init(self, Async_IS_RESOLVED_OR, first, orelse);
}

void
Async_ResolvedOr_init_move(
        Async*  self,
        Async*  other)
{
    binary_init_move(self, Async_IS_RESOLVED_OR, other);
}

void
Async_ResolvedOr_clear(
        Async*  self)
{
    binary_clear(self, Async_IS_RESOLVED_OR);
}

// ResolvedThen

void
Async_ResolvedThen_init(
        Async*  self,
        Async*  first,
        Async*  then)
{
    binary_init(self, Async_IS_RESOLVED_THEN, first, then);
}

void
Async_ResolvedThen_init_move(
        Async*  self,
        Async*  other)
{
    binary_init_move(self, Async_IS_RESOLVED_THEN, other);
}

void
Async_ResolvedThen_clear(
        Async*  self)
{
    binary_clear(self, Async_IS_RESOLVED_THEN);
}

// ValueOr

void
Async_ValueOr_init(
        Async*  self,
        Async*  first,
        Async*  orelse)
{
    binary_init(self, Async_IS_VALUE_OR, first, orelse);
}

void
Async_ValueOr_init_move(
        Async*  self,
        Async*  other)
{
    binary_init_move(self, Async_IS_VALUE_OR, other);
}

void
Async_ValueOr_clear(
        Async*  self)
{
    binary_clear(self, Async_IS_VALUE_OR);
}

// ValueThen

void
Async_ValueThen_init(
        Async*  self,
        Async*  first,
        Async*  then)
{
    binary_init(self, Async_IS_VALUE_THEN, first, then);
}

void
Async_ValueThen_init_move(
        Async*  self,
        Async*  other)
{
    binary_init_move(self, Async_IS_VALUE_THEN, other);
}

void
Async_ValueThen_clear(
        Async*  self)
{
    binary_clear(self, Async_IS_VALUE_THEN);
}

// Cancel

void
Async_Cancel_init(
        Async* self)
{
    assert(self);
    assert(self->type == Async_IS_UNINITIALIZED);

    self->type = Async_IS_CANCEL;
}

void
Async_Cancel_init_move(
        Async*  self,
        Async*  other)
{
    Async_Cancel_init(self);
    Async_Cancel_clear(other);
}

void
Async_Cancel_clear(
        Async* self)
{
    assert(self);
    assert(self->type == Async_IS_CANCEL);

    self->type = Async_IS_UNINITIALIZED;
}

// Error

void
Async_Error_init(
        Async*          self,
        Destructible    error)
{
    assert(self);
    assert(self->type == Async_IS_UNINITIALIZED);
    assert(error.vtable);

    self->type = Async_IS_ERROR;
    self->as_error = error;
}

void
Async_Error_init_move(
        Async*  self,
        Async*  other)
{
    assert(self);
    assert(self->type == Async_IS_UNINITIALIZED);
    assert(other);
    assert(other->type == Async_IS_ERROR);

    self->type = Async_IS_ERROR;
    other->type = Async_IS_UNINITIALIZED;
    Destructible_init_move(&self->as_error, &other->as_error);
}

void
Async_Error_clear(
        Async*  self)
{
    assert(self);
    assert(self->type == Async_IS_ERROR);

    self->type = Async_IS_UNINITIALIZED;
    Destructible_clear(&self->as_error);
}

// Value

void
Async_Value_init(
        Async*                          self,
        DestructibleTuple*              values)
{
    assert(self);
    assert(self->type == Async_IS_UNINITIALIZED);

    if (ASYNC_TRAMPOLINE_DEBUG)
    {
        size_t size = (values) ? values->size : 0;
        ASYNC_LOG_DEBUG(
                "init Async %p to Values: values = %p { size = %zu }",
                self, values, size);
        for (size_t i = 0; i < size; i++)
            ASYNC_LOG_DEBUG("  - values %p",
                    DestructibleTuple_at(values, i));
    }

    self->type = Async_IS_VALUE;
    self->as_value = values;
}

void
Async_Value_init_move(
        Async*  self,
        Async*  other)
{
    assert(self);
    assert(self->type == Async_IS_UNINITIALIZED);
    assert(other);
    assert(other->type == Async_IS_VALUE);
    assert(other->refcount == 1);

    self->type = Async_IS_VALUE;
    self->as_value = other->as_value;
    other->as_value = NULL;

    Async_Value_clear(other);
}

void
Async_Value_clear(
        Async*  self)
{
    assert(self);
    assert(self->type == Async_IS_VALUE);

    self->type = Async_IS_UNINITIALIZED;

    if (self->as_value)
        DestructibleTuple_clear(self->as_value);
}
