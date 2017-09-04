#include "Async.h"
#include "Scheduler.h"

inline static
void
ref_if_not_null(Async* self)
{
    if (self) Async_ref(self);
}

#define EVAL_RETURN(next_async, blocked_async) \
    (void)  (ref_if_not_null(*next = next_async),                           \
             ref_if_not_null(*blocked = blocked_async))

void
Async_run_until_completion(
        Async* async)
{
    ASYNC_LOG_DEBUG("loop for Async %p", async);

    dTHX;

    Async_Trampoline_Scheduler* scheduler =
        Async_Trampoline_Scheduler_new_with_default_capacity(aTHX);

    SAVEDESTRUCTOR(Async_Trampoline_Scheduler_unref, scheduler);

    Async_Trampoline_Scheduler_enqueue_without_dependencies(
            aTHX_ scheduler, async);

    while (1)
    {
        Async* top = Async_Trampoline_Scheduler_dequeue(aTHX_ scheduler);

        if (!top)
            break;

        ENTER;

        Async trap;
        Async* next = &trap;
        Async* blocked = &trap;
        Async_eval(top, &next, &blocked);

        assert(next != &trap);
        assert(blocked != &trap);

        if (blocked)
            assert(next);

        if (next)
        {
            Async_Trampoline_Scheduler_enqueue_without_dependencies(
                    aTHX_ scheduler, next);

            if (blocked)
            {
                Async_Trampoline_Scheduler_block_on(
                        aTHX_ scheduler, next, blocked);
            }
        }

        if (top != next && top != blocked)
        {
            assert(Async_has_category(top, Async_CATEGORY_COMPLETE));
            Async_Trampoline_Scheduler_complete(aTHX_ scheduler, top);
        }

        LEAVE;

        Async_unref(top);
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

#define ENSURE_DEPENDENCY(self, dependency) do {                            \
    if (!Async_has_category((dependency), Async_CATEGORY_COMPLETE))         \
        return EVAL_RETURN((dependency), (self));                           \
} while (0)

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

        ENSURE_DEPENDENCY(self, dependency);

        if (!Async_has_category(dependency, Async_CATEGORY_COMPLETE))
        {
            return EVAL_RETURN(dependency, self);
        }

        if (!Async_has_type(dependency, Async_IS_VALUE))
        {
            Async_unify(self, dependency);
            return EVAL_RETURN(NULL, NULL);
        }

        assert(dependency->type == Async_IS_VALUE);
        values = dependency->as_value;
    }

    Async* result = self->as_thunk.callback(&self->as_thunk.context, values);
    assert(result);

    Async_unify(self, result);
    Async_unref(result);

    return EVAL_RETURN(self, NULL);
}

static
Async*
select_if_either_has_type(Async* left, Async* right, enum Async_Type type)
{
    if (Async_has_type(left, type))
        return left;
    if (Async_has_type(right, type))
        return right;
    return NULL;
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

    Async* left     = Async_Ptr_fold(&self->as_binary.left);
    Async* right    = Async_Ptr_fold(&self->as_binary.right);

    Async* selected;
    if (    (selected = select_if_either_has_type(left, right, Async_IS_CANCEL))
        ||  (selected = select_if_either_has_type(left, right, Async_IS_ERROR)))
    {
        Async_unify(self, selected);
        return EVAL_RETURN(NULL, NULL);
    }

    ENSURE_DEPENDENCY(self, left);
    ENSURE_DEPENDENCY(self, right);

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
    return EVAL_RETURN(NULL, NULL);
}

enum ControlFlow {
    CONTROL_FLOW_THEN,
    CONTROL_FLOW_OR,
};

static
void
eval_control_flow_op(
        Async*  self,
        enum Async_Type self_type,
        enum Async_Type decision_type,
        enum ControlFlow control_flow,
        Async** next,
        Async** blocked)
{
    assert(self);
    assert(self->type == self_type);

    Async* left = self->as_binary.left;
    Async* right = self->as_binary.right;

    ENSURE_DEPENDENCY(self, left);

    bool stay_left;
    switch (control_flow)
    {
        case CONTROL_FLOW_THEN:
            stay_left = !Async_has_category(left, decision_type);
            break;
        case CONTROL_FLOW_OR:
            stay_left = Async_has_category(left, decision_type);
            break;
    }

    if (stay_left)
    {
        Async_unify(self, left);
        return EVAL_RETURN(NULL, NULL);
    }
    else
    {
        Async_unify(self, right);
        return EVAL_RETURN(self, NULL);
    }
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
            eval_control_flow_op(
                    self,
                    Async_IS_COMPLETE_THEN,
                    Async_CATEGORY_COMPLETE,
                    CONTROL_FLOW_THEN,
                    next,
                    blocked);
            break;
        case Async_IS_RESOLVED_OR:
            eval_control_flow_op(
                    self,
                    Async_IS_RESOLVED_OR,
                    Async_CATEGORY_RESOLVED,
                    CONTROL_FLOW_OR,
                    next,
                    blocked);
            break;
        case Async_IS_RESOLVED_THEN:
            eval_control_flow_op(
                    self,
                    Async_IS_RESOLVED_THEN,
                    Async_CATEGORY_RESOLVED,
                    CONTROL_FLOW_THEN,
                    next,
                    blocked);
            break;
        case Async_IS_VALUE_OR:
            eval_control_flow_op(
                    self,
                    Async_IS_VALUE_OR,
                    Async_IS_VALUE,
                    CONTROL_FLOW_OR,
                    next,
                    blocked);
            break;
        case Async_IS_VALUE_THEN:
            eval_control_flow_op(
                    self,
                    Async_IS_VALUE_THEN,
                    Async_IS_VALUE,
                    CONTROL_FLOW_THEN,
                    next,
                    blocked);
            break;

        case Async_CATEGORY_COMPLETE:
            assert(0);
            break;
        case Async_IS_CANCEL:  // already complete
            EVAL_RETURN(NULL, NULL);
            break;

        case Async_CATEGORY_RESOLVED:
            assert(0);
            break;
        case Async_IS_ERROR:  // already complete
            EVAL_RETURN(NULL, NULL);
            break;
        case Async_IS_VALUE:  // already complete
            EVAL_RETURN(NULL, NULL);
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

