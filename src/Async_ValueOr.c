#include "Async.h"

void
Async_ValueOr_eval(
        Async*  self,
        Async** next,
        Async** blocked)
{
    assert(self);
    assert(self->type == Async_IS_VALUE_OR);

    Async* left = self->as_binary.left;
    Async* right = self->as_binary.right;

    if (!Async_has_category(left, Async_CATEGORY_COMPLETE))
    {
        Async_ref(*next = left);
        Async_ref(*blocked = self);
        return;
    }

    if (!Async_has_type(left, Async_IS_VALUE))
    {
        Async_unify(self, right);
        Async_ref(*next = self);
        return;
    }

    Async_unify(self, left);
    return;
}
