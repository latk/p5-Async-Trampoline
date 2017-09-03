#include "Async.h"

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
