#include "Async.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

Async*
Async_new()
{
    Async* self = (Async*) malloc(sizeof(Async));

    if (!self)
        return NULL;

    ASYNC_LOG_DEBUG("created new Async at %p\n", self);

    // Zero out the memory.
    // This is important as zero is seen as "uninitialized"
    // by subsequent functions.
    memset((void*) self, 0, sizeof(Async));

    self->type = Async_Type::IS_UNINITIALIZED;
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

    ASYNC_LOG_DEBUG("deleting Async at %p\n", self);

    Async_clear(self);

    assert(self->type == Async_Type::IS_UNINITIALIZED);

    free(self);
}

auto Async::ptr_follow() -> Async&
{
    if (type != Async_Type::IS_PTR)
        return *this;

    // flatten the pointer until we reach something concrete
    AsyncRef& ptr = as_ptr;
    while (ptr->type == Async_Type::IS_PTR)
        ptr = ptr->as_ptr;

    return ptr.get();
}
