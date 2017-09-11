#include "Async.h"
#include "Scheduler.h"

#define EVAL_RETURN(next_async, blocked_async) \
    (void)  (next = next_async, blocked = blocked_async)

void
Async_run_until_completion(
        Async* async)
{
    ASYNC_LOG_DEBUG("loop for Async %p\n", async);

    Async_Trampoline_Scheduler scheduler{};

    scheduler.enqueue(async);

    while (scheduler.queue_size() > 0)
    {
        Async* top = scheduler.dequeue();

        if (!top)
            break;

        Async trap;
        AsyncRef next = &trap;
        AsyncRef blocked = &trap;
        Async_eval(top, next, blocked);

        assert(next.decay() != &trap);
        assert(blocked.decay() != &trap);

        if (blocked)
            assert(next);

        if (next)
        {
            scheduler.enqueue(next.decay());

            if (blocked)
            {
                scheduler.block_on(next.decay(), blocked.decay());
            }
        }

        if (top != next.decay() && top != blocked.decay())
        {
            assert(Async_has_category(top, Async_Type::CATEGORY_COMPLETE));
            scheduler.complete(top);
        }

        top->unref();
    }

    ASYNC_LOG_DEBUG("loop complete\n");
}

// Type-specific cases

static
void
Async_RawThunk_eval(
        Async*  self,
        AsyncRef& next,
        AsyncRef& blocked)
{
    assert(self);
    assert(self->type == Async_Type::IS_RAWTHUNK);

    assert(0);  // TODO not implemented
}

#define ENSURE_DEPENDENCY(self, dependency) do {                            \
    if (!(dependency)->has_category(Async_Type::CATEGORY_COMPLETE))         \
        return EVAL_RETURN((dependency), (self));                           \
} while (0)

static
void
Async_Thunk_eval(
        Async*  self,
        AsyncRef& next,
        AsyncRef& blocked)
{
    assert(self);
    assert(self->type == Async_Type::IS_THUNK);

    ASYNC_LOG_DEBUG(
            "running Thunk %p: callback=%p context.data=%p dependency=%p\n",
            self,
            self->as_thunk.callback,
            self->as_thunk.context.data,
            self->as_thunk.dependency.decay());

    AsyncRef dependency = self->as_thunk.dependency;
    DestructibleTuple default_value{};
    DestructibleTuple const* values = &default_value;
    if (dependency)
    {
        dependency.fold();

        ENSURE_DEPENDENCY(self, dependency);

        if (!dependency->has_type(Async_Type::IS_VALUE))
        {
            Async_unify(self, dependency.decay());
            return EVAL_RETURN(NULL, NULL);
        }

        assert(dependency->type == Async_Type::IS_VALUE);
        values = &dependency->as_value;
    }

    AsyncRef result = self->as_thunk.callback(
            std::move(self->as_thunk.context), *values);
    assert(result);

    Async_unify(self, result.decay());

    return EVAL_RETURN(self, nullptr);
}

static
Async*
select_if_either_has_type(AsyncRef& left, AsyncRef& right, Async_Type type)
{
    if (left->has_type(type))
        return left.decay();
    if (right->has_type(type))
        return right.decay();
    return nullptr;
}

static
void
Async_Concat_eval(
        Async*  self,
        AsyncRef& next,
        AsyncRef& blocked)
{
    assert(self);
    assert(self->type == Async_Type::IS_CONCAT);

    auto& left  = self->as_binary.left.fold();
    auto& right = self->as_binary.right.fold();

    for (Async_Type type : { Async_Type::IS_CANCEL, Async_Type::IS_ERROR })
    {
        if(Async* selected  = select_if_either_has_type(left, right, type))
        {
            Async_unify(self, selected);
            return EVAL_RETURN(nullptr, nullptr);
        }
    }

    ENSURE_DEPENDENCY(self, left);
    ENSURE_DEPENDENCY(self, right);

    assert(left->type   == Async_Type::IS_VALUE);
    assert(right->type  == Async_Type::IS_VALUE);

    assert(left->as_value.vtable == right->as_value.vtable);

    auto vtable = left->as_value.vtable;
    size_t size = left->as_value.size + right->as_value.size;

    DestructibleTuple tuple {vtable, size};

    // move or copy the values,
    // depending on left/right refcount

    size_t output_i = 0;
    for (Async* source : { left.decay(), right.decay() })
    {
        Destructible (DestructibleTuple::* copy_or_move)(size_t) =
            (source->refcount == 1)
            ? &DestructibleTuple::move_from
            : &DestructibleTuple::copy_from;
        DestructibleTuple& input = source->as_value;
        for (size_t input_i = 0; input_i < input.size; input_i++, output_i++)
        {
            Destructible temp = (input.*copy_or_move)(input_i);
            tuple.set(output_i, std::move(temp));
        }
    }

    Async_Concat_clear(self);
    Async_Value_init(self, std::move(tuple));
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
        AsyncRef& next,
        AsyncRef& blocked)
{
    assert(self);
    assert(self->type == self_type);

    Async* left = self->as_binary.left.decay();
    Async* right = self->as_binary.right.decay();

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
        AsyncRef& next,
        AsyncRef& blocked)
{
    ASYNC_LOG_DEBUG(
            "running Async %p (%2d %s)\n",
            self,
            static_cast<int>(self->type),
            Async_Type_name(self->type));

    switch (self->type) {
        case Async_Type::IS_UNINITIALIZED:
            assert(0);
            break;

        case Async_Type::CATEGORY_INITIALIZED:
            assert(0);
            break;
        case Async_Type::IS_PTR:
            Async_eval(Async_Ptr_follow(self), next, blocked);
            break;
        case Async_Type::IS_RAWTHUNK:
            Async_RawThunk_eval(
                    self, next, blocked);
            break;
        case Async_Type::IS_THUNK:
            Async_Thunk_eval(
                    self, next, blocked);
            break;
        case Async_Type::IS_CONCAT:
            Async_Concat_eval(
                    self, next, blocked);
            break;
        case Async_Type::IS_COMPLETE_THEN:
            eval_control_flow_op(
                    self,
                    Async_Type::IS_COMPLETE_THEN,
                    Async_Type::CATEGORY_COMPLETE,
                    CONTROL_FLOW_THEN,
                    next,
                    blocked);
            break;
        case Async_Type::IS_RESOLVED_OR:
            eval_control_flow_op(
                    self,
                    Async_Type::IS_RESOLVED_OR,
                    Async_Type::CATEGORY_RESOLVED,
                    CONTROL_FLOW_OR,
                    next,
                    blocked);
            break;
        case Async_Type::IS_RESOLVED_THEN:
            eval_control_flow_op(
                    self,
                    Async_Type::IS_RESOLVED_THEN,
                    Async_Type::CATEGORY_RESOLVED,
                    CONTROL_FLOW_THEN,
                    next,
                    blocked);
            break;
        case Async_Type::IS_VALUE_OR:
            eval_control_flow_op(
                    self,
                    Async_Type::IS_VALUE_OR,
                    Async_Type::IS_VALUE,
                    CONTROL_FLOW_OR,
                    next,
                    blocked);
            break;
        case Async_Type::IS_VALUE_THEN:
            eval_control_flow_op(
                    self,
                    Async_Type::IS_VALUE_THEN,
                    Async_Type::IS_VALUE,
                    CONTROL_FLOW_THEN,
                    next,
                    blocked);
            break;

        case Async_Type::CATEGORY_COMPLETE:
            assert(0);
            break;
        case Async_Type::IS_CANCEL:  // already complete
            EVAL_RETURN(NULL, NULL);
            break;

        case Async_Type::CATEGORY_RESOLVED:
            assert(0);
            break;
        case Async_Type::IS_ERROR:  // already complete
            EVAL_RETURN(NULL, NULL);
            break;
        case Async_Type::IS_VALUE:  // already complete
            EVAL_RETURN(NULL, NULL);
            break;

        default:
            assert(0);
    }

    ASYNC_LOG_DEBUG(
            "... %p result: next=%p blocked=%p\n",
            self,
            next,
            blocked);
}

