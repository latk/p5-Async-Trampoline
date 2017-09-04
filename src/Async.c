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
    return target->type >= category;
}

Async*
Async_Ptr_follow(
        Async* self)
{
    assert(self);

    if (self->type != Async_IS_PTR)
        return self;

    // flatten the pointer until we reach something concrete
    Async* ptr = self->as_ptr;
    while (ptr->type == Async_IS_PTR)
    {
        Async* next = ptr->as_ptr;
        Async_ref(next);
        Async_unref(ptr);
        ptr = next;
    }
    self->as_ptr = ptr;

    return ptr;
}
