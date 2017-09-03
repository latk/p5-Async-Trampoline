#include "Async.h"

void
Async_Concat_init(
        Async*  self,
        Async*  left,
        Async*  right)
{
    assert(self);
    assert(self->type == Async_IS_UNINITIALIZED);
    assert(left);
    assert(right);

    self->type = Async_IS_CONCAT;
    self->as_binary.left = left;
    self->as_binary.right = right;

    Async_ref(left);
    Async_ref(right);
}

void
Async_Concat_init_move(
        Async*  self,
        Async*  other)
{
    assert(self);
    assert(self->type == Async_IS_UNINITIALIZED);
    assert(other);
    assert(other->type == Async_IS_CONCAT);

    self->type = Async_IS_CONCAT;
    self->as_binary = other->as_binary;
    other->as_binary.left = NULL;
    other->as_binary.right = NULL;
}

void
Async_Concat_clear(
        Async*  self)
{
    assert(self);
    assert(self->type == Async_IS_CONCAT);

    Async_unref(self->as_binary.left);
    Async_unref(self->as_binary.right);

    self->as_binary.left = NULL;
    self->as_binary.right = NULL;
}

void
Async_Concat_eval(
        Async*  self,
        Async** next,
        Async** blocked)
{
    assert(self);
    assert(self->type == Async_IS_CONCAT);

    Async* left = self->as_binary.left;
    Async* right = self->as_binary.right;

    if (!Async_has_category(left, Async_CATEGORY_COMPLETE))
    {
        Async_ref(*next = left);
        Async_ref(*blocked = self);
        return;
    }

    if (!Async_has_category(right, Async_CATEGORY_COMPLETE))
    {
        Async_ref(*next = right);
        Async_ref(*blocked = self);
        return;
    }

    // After this point, left and right are resolved.
    // We can therefore follow them if they are pointers.
    left    = Async_Ptr_follow(left);
    right   = Async_Ptr_follow(right);

    if (Async_has_type(left, Async_IS_CANCEL)
            || Async_has_type(right, Async_IS_CANCEL))
    {
        Async_Concat_clear(self);
        Async_Cancel_init(self);
        return;
    }

    if (Async_has_type(left, Async_IS_ERROR))
    {
        Async_unify(self, left);
        return;
    }

    if (Async_has_type(right, Async_IS_ERROR))
    {
        Async_unify(self, right);
        return;
    }

    assert(left->type   == Async_IS_VALUE);
    assert(right->type  == Async_IS_VALUE);

    assert(left->as_value->vtable == right->as_value->vtable);

    Destructible_Vtable* vtable = left->as_value->vtable;
    size_t size = left->as_value->size + right->as_value->size;

    DestructibleTuple* tuple = DestructibleTuple_new(vtable, size);

    // move or copy the values,
    // depending on left/right refcount

    Async* sources[] = { left, right };

    size_t output_i = 0;
    for (size_t source_i = 0; source_i < 2; source_i++) {
        Async* source = sources[source_i];
        Destructible (*copy_or_move)(DestructibleTuple*, size_t) =
            (source->refcount == 1)
            ? DestructibleTuple_move_from
            : DestructibleTuple_copy_from;
        DestructibleTuple* input = source->as_value;
        for (size_t input_i = 0; input_i < input->size; input_i++, output_i++)
        {
            Destructible temp = copy_or_move(input, input_i);
            DestructibleTuple_move_into(tuple, output_i, &temp);
        }
    }

    Async_Concat_clear(self);
    Async_Value_init(self, tuple);
}
