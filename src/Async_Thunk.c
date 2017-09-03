#include "Async.h"

void
Async_Thunk_init(
        Async*              self,
        Async_ThunkCallback callback,
        Destructible        context,
        Async*              dependency)
{
    ASYNC_LOG_DEBUG(
            "init Async %p to Thunk: callback=%p context.data=%p dependency=%p",
            self, callback, context.data, dependency);

    assert(self);
    assert(self->type == Async_IS_UNINITIALIZED);
    assert(callback);

    if (dependency)
        Async_ref(dependency);

    self->type = Async_IS_THUNK;
    self->as_thunk.dependency = dependency;
    self->as_thunk.callback = callback;
    Destructible_init_move(
            &self->as_thunk.context, &context);
}

void
Async_Thunk_init_move(
        Async*  self,
        Async*  other)
{
    assert(self);
    assert(self->type == Async_IS_UNINITIALIZED);
    assert(other);
    assert(other->type == Async_IS_THUNK);

    self->type = Async_IS_THUNK;
    other->type = Async_IS_UNINITIALIZED;

    self->as_thunk.dependency = other->as_thunk.dependency;
    other->as_thunk.dependency = NULL;

    self->as_thunk.callback = other->as_thunk.callback;
    other->as_thunk.callback = NULL;

    Destructible_init_move(&self->as_thunk.context, &other->as_thunk.context);
}

void
Async_Thunk_clear(
        Async*  self)
{
    assert(self);
    assert(self->type == Async_IS_THUNK);

    Async* dependency = self->as_thunk.dependency;
    if (dependency)
        Async_unref(dependency);

    self->type = Async_IS_UNINITIALIZED;
    self->as_thunk.dependency = NULL;
    self->as_thunk.callback = NULL;
    Destructible_clear(&self->as_thunk.context);
}

void
Async_Thunk_eval(
        Async*  self,
        Async** next,
        Async** blocked)
{
    assert(self);
    assert(self->type == Async_IS_THUNK);

    ASYNC_LOG_DEBUG(
            "running Thunk %p: callback=%p context.data=%p dependency=%p",
            self,
            self->as_thunk.callback,
            self->as_thunk.context.data,
            self->as_thunk.dependency);

    Async* dependency = self->as_thunk.dependency;
    DestructibleTuple* values = NULL;
    if (dependency)
    {

        dependency = Async_Ptr_follow(dependency);

        if (dependency->type < Async_CATEGORY_COMPLETE)
        {
            Async_ref(*next = dependency);
            Async_ref(*blocked = self);
            return;
        }

        if (dependency->type == Async_IS_CANCEL)
        {
            Async_Thunk_clear(self);
            Async_Cancel_init(self);
            return;
        }

        if (dependency->type == Async_IS_ERROR)
        {
            Async_ref(dependency);
            Async_Thunk_clear(self);
            Async_Ptr_init(self, dependency);
            Async_unref(dependency);
            return;
        }

        assert(dependency->type == Async_IS_VALUE);
        values = dependency->as_value;
    }

    Async* result = self->as_thunk.callback(&self->as_thunk.context, values);
    assert(result);

    Async_ref(result);
    Async_Thunk_clear(self);
    Async_Ptr_init(self, result);
    Async_unref(result);

    Async_ref(*next = self);
}
