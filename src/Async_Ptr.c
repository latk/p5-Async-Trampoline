#include "Async.h"

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
