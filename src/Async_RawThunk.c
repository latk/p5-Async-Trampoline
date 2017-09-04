#include "Async.h"

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
