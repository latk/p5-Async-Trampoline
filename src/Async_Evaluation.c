#include "Async.h"
#include "Scheduler.h"

void
Async_run_until_completion(
        Async* async)
{
    ASYNC_LOG_DEBUG("loop for Async %p", async);

    dTHX;

    Async_Trampoline_Scheduler* scheduler =
        Async_Trampoline_Scheduler_new_with_default_capacity(aTHX);

    SAVEDESTRUCTOR(Async_Trampoline_Scheduler_unref, scheduler);

    SV* async_sv = newSV(0);
    SAVEFREESV(async_sv);
    sv_setref_pv(async_sv, "Async::Trampoline", (void*) async);
    Async_ref(async);

    Async_Trampoline_Scheduler_enqueue_without_dependencies(
            aTHX_ scheduler, async_sv);

    while (1)
    {
        SV* top_sv = top_sv = Async_Trampoline_Scheduler_dequeue(aTHX_ scheduler);

        if (!top_sv)
            break;

        ENTER;

        assert(sv_isa(top_sv, "Async::Trampoline"));
        Async* top = (Async*) SvIV(SvRV(top_sv));
        assert(top);

        SAVEFREESV(top_sv);

        Async* next = NULL;
        Async* blocked = NULL;
        Async_eval(top, &next, &blocked);

        if (blocked)
            assert(next);

        if (next)
        {
            SV* next_sv = newSV(0);
            SAVEFREESV(next_sv);
            sv_setref_pv(next_sv, "Async::Trampoline", (void*) next);

            Async_Trampoline_Scheduler_enqueue_without_dependencies(
                    aTHX_ scheduler, next_sv);

            if (blocked)
            {
                SV* blocked_sv = newSV(0);
                SAVEFREESV(blocked_sv);
                sv_setref_pv(blocked_sv, "Async::Trampoline", (void*) blocked);

                Async_Trampoline_Scheduler_block_on(
                        aTHX_ scheduler, next_sv, blocked_sv);
            }
        }

        if (top != next && top != blocked)
        {
            assert(Async_has_category(top, Async_CATEGORY_COMPLETE));
            Async_Trampoline_Scheduler_complete(aTHX_ scheduler, top_sv);
        }

        LEAVE;
    }

    ASYNC_LOG_DEBUG("loop complete");
}

// Type-specific cases

static
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

static
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

static
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

static
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

static
void
Async_ResolvedOr_eval(
        Async*  self,
        Async** next,
        Async** blocked)
{
    assert(self);
    assert(self->type == Async_IS_RESOLVED_OR);

    Async* left = self->as_binary.left;
    Async* right = self->as_binary.right;

    if (!Async_has_category(left, Async_CATEGORY_COMPLETE))
    {
        Async_ref(*next = left);
        Async_ref(*blocked = self);
        return;
    }

    if (!Async_has_category(left, Async_CATEGORY_RESOLVED))
    {
        Async_unify(self, right);
        Async_ref(*next = self);
        return;
    }

    Async_unify(self, left);
    return;
}

static
void
Async_ResolvedThen_eval(
        Async*  self,
        Async** next,
        Async** blocked)
{
    assert(self);
    assert(self->type == Async_IS_RESOLVED_THEN);

    Async* left = self->as_binary.left;
    Async* right = self->as_binary.right;

    if (!Async_has_category(left, Async_CATEGORY_COMPLETE))
    {
        Async_ref(*next = left);
        Async_ref(*blocked = self);
        return;
    }

    if (!Async_has_category(left, Async_CATEGORY_RESOLVED))
    {
        Async_unify(self, left);
        return;
    }

    Async_unify(self, right);
    Async_ref(*next = self);
    return;
}

static
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

static
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

// Polymorphic

void
Async_eval(
        Async*  self,
        Async** next,
        Async** blocked)
{
    ASYNC_LOG_DEBUG(
            "running Async %p (%2d %s)",
            self,
            self->type,
            Async_Type_name(self->type));

    switch (self->type) {
        case Async_IS_UNINITIALIZED:
            assert(0);
            break;

        case Async_CATEGORY_INITIALIZED:
            assert(0);
            break;
        case Async_IS_PTR:
            Async_eval(Async_Ptr_follow(self), next, blocked);
            break;
        case Async_IS_RAWTHUNK:
            Async_RawThunk_eval(
                    self, next, blocked);
            break;
        case Async_IS_THUNK:
            Async_Thunk_eval(
                    self, next, blocked);
            break;
        case Async_IS_CONCAT:
            Async_Concat_eval(
                    self, next, blocked);
            break;
        case Async_IS_COMPLETE_THEN:
            Async_CompleteThen_eval(
                    self, next, blocked);
            break;
        case Async_IS_RESOLVED_OR:
            Async_ResolvedOr_eval(
                    self, next, blocked);
            break;
        case Async_IS_RESOLVED_THEN:
            Async_ResolvedThen_eval(
                    self, next, blocked);
            break;
        case Async_IS_VALUE_OR:
            Async_ValueOr_eval(
                    self, next, blocked);
            break;
        case Async_IS_VALUE_THEN:
            Async_ValueThen_eval(
                    self, next, blocked);
            break;

        case Async_CATEGORY_COMPLETE:
            assert(0);
            break;
        case Async_IS_CANCEL:  // already complete
            break;

        case Async_CATEGORY_RESOLVED:
            assert(0);
            break;
        case Async_IS_ERROR:  // already complete
            break;
        case Async_IS_VALUE:  // already complete
            break;

        default:
            assert(0);
    }

    ASYNC_LOG_DEBUG(
            "... %p result: next=%p blocked=%p",
            self,
            *next,
            *blocked);
}

