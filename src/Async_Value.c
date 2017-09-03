#include "Async.h"

void
Async_Value_init(
        Async*                          self,
        DestructibleTuple*              values)
{
    assert(self);
    assert(self->type == Async_IS_UNINITIALIZED);

    if (ASYNC_TRAMPOLINE_DEBUG)
    {
        size_t size = (values) ? values->size : 0;
        ASYNC_LOG_DEBUG(
                "init Async %p to Values: values = %p { size = %zu }",
                self, values, size);
        for (size_t i = 0; i < size; i++)
            ASYNC_LOG_DEBUG("  - values %p",
                    DestructibleTuple_at(values, i));
    }

    self->type = Async_IS_VALUE;
    self->as_value = values;
}

void
Async_Value_init_move(
        Async*  self,
        Async*  other)
{
    assert(self);
    assert(self->type == Async_IS_UNINITIALIZED);
    assert(other);
    assert(other->type == Async_IS_VALUE);
    assert(other->refcount == 1);

    self->type = Async_IS_VALUE;
    self->as_value = other->as_value;
    other->as_value = NULL;

    Async_Value_clear(other);
}

void
Async_Value_clear(
        Async*  self)
{
    assert(self);
    assert(self->type == Async_IS_VALUE);

    self->type = Async_IS_UNINITIALIZED;

    if (self->as_value)
        DestructibleTuple_clear(self->as_value);
}
