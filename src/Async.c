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

