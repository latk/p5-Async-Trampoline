#include "Async.h"

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

void
Async_RawThunk_eval(
        Async*  self,
        Async** next,
        Async** blocked)
{
    assert(self);
    assert(self->type == Async_IS_RAWTHUNK);

    assert(0);  // TODO not implemented
}
