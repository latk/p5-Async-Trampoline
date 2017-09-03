#include "EventLoop.h"
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
