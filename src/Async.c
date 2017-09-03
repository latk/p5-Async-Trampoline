#include "Async.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

Async*
Async_new()
{
    Async* self = malloc(sizeof(Async));

    if (!self)
        return NULL;

    ASYNC_LOG_DEBUG("created new Async at %p", self);

    // Zero out the memory.
    // This is important as zero is seen as "uninitialized"
    // by subsequent functions.
    memset((void*) self, 0, sizeof(Async));

    self->type = Async_IS_UNINITIALIZED;
    self->refcount = 1;

    return self;
}

void
Async_ref(
        Async* self)
{
    assert(self);

    self->refcount++;
}

void
Async_unref(
        Async* self)
{
    assert(self);

    self->refcount--;

    if (self->refcount)
        return;

    ASYNC_LOG_DEBUG("deleting Async at %p", self);

    Async_clear(self);

    assert(self->type == Async_IS_UNINITIALIZED);

    free(self);
}

bool
Async_has_type(
        Async* self,
        enum Async_Type type)
{
    assert(self);
    Async* target = Async_Ptr_follow(self);
    return target->type == type;
}

bool
Async_has_category(
        Async* self,
        enum Async_Type category)
{
    assert(self);
    Async* target = Async_Ptr_follow(self);
    return target->type > category;
}



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
Async_eval(
        Async*  self,
        Async** next,
        Async** blocked)
{
    ASYNC_LOG_DEBUG(
            "running Async %p (%2d %s)",
            self,
            self->type,
            Async_Type_name(self->type));

    switch (self->type) {
        case Async_IS_UNINITIALIZED:
            assert(0);
            break;

        case Async_CATEGORY_INITIALIZED:
            assert(0);
            break;
        case Async_IS_PTR:
            Async_eval(Async_Ptr_follow(self), next, blocked);
            break;
        case Async_IS_RAWTHUNK:
            Async_RawThunk_eval(
                    self, next, blocked);
            break;
        case Async_IS_THUNK:
            Async_Thunk_eval(
                    self, next, blocked);
            break;
        case Async_IS_CONCAT:
            Async_Concat_eval(
                    self, next, blocked);
            break;
        case Async_IS_COMPLETE_THEN:
            Async_CompleteThen_eval(
                    self, next, blocked);
            break;
        case Async_IS_RESOLVED_OR:
            Async_ResolvedOr_eval(
                    self, next, blocked);
            break;
        case Async_IS_RESOLVED_THEN:
            Async_ResolvedThen_eval(
                    self, next, blocked);
            break;
        case Async_IS_VALUE_OR:
            Async_ValueOr_eval(
                    self, next, blocked);
            break;
        case Async_IS_VALUE_THEN:
            Async_ValueThen_eval(
                    self, next, blocked);
            break;

        case Async_CATEGORY_COMPLETE:
            assert(0);
            break;
        case Async_IS_CANCEL:  // already complete
            break;

        case Async_CATEGORY_RESOLVED:
            assert(0);
            break;
        case Async_IS_ERROR:  // already complete
            break;
        case Async_IS_VALUE:  // already complete
            break;

        default:
            assert(0);
    }

    ASYNC_LOG_DEBUG(
            "... %p result: next=%p blocked=%p",
            self,
            *next,
            *blocked);
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

void
Async_Binary_init(
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

void
Async_Binary_init_move(
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

void
Async_Binary_clear(
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
