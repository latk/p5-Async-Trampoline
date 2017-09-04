#include "Async.h"

void
Async_CompleteThen_eval(
        Async*  self,
        Async** next,
        Async** blocked)
{
    assert(self);
    assert(self->type == Async_IS_COMPLETE_THEN);

    Async* left = self->as_binary.left;
    Async* right = self->as_binary.right;

    if (!Async_has_category(left, Async_CATEGORY_COMPLETE))
    {
        Async_ref(*next = left);
        Async_ref(*blocked = self);
        return;
    }
    else {
        Async_unify(self, right);
        Async_ref(*next = self);
        return;
    }
}
