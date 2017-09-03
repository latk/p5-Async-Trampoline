#include "Async.h"

void
Async_ValueThen_init(
        Async*  self,
        Async*  first,
        Async*  then)
{
    Async_Binary_init(self, Async_IS_VALUE_THEN, first, then);
}

void
Async_ValueThen_init_move(
        Async*  self,
        Async*  other)
{
    Async_Binary_init_move(self, Async_IS_VALUE_THEN, other);
}

void
Async_ValueThen_clear(
        Async*  self)
{
    Async_Binary_clear(self, Async_IS_VALUE_THEN);
}

void
Async_ValueThen_eval(
        Async*  self,
        Async** next,
        Async** blocked)
{
    assert(self);
    assert(self->type = Async_IS_VALUE_THEN);

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
        Async_unify(self, left);
        return;
    }

    Async_unify(self, right);
    Async_ref(*next = self);
    return;
}
