#include "Async.h"

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
