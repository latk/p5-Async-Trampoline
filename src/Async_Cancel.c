#include "Async.h"
#include <assert.h>
#define UNUSED(x) (void)(x)

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

void
Async_cancel_eval(
        Async*  self,
        Async** next,
        Async** blocked)
{
    UNUSED(self);
    UNUSED(next);
    UNUSED(blocked);
    return;
}
