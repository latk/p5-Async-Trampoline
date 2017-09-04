#include "Async.h"
#include <assert.h>
#define UNUSED(x) (void)(x)

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
