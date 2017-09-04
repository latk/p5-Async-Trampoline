#define PERL_NO_GET_CONTEXT
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "Scheduler.h"

MODULE = Async::Trampoline::Scheduler PACKAGE = Async::Trampoline::Scheduler PREFIX = Async_Trampoline_Scheduler_

Async_Trampoline_Scheduler*
Async_Trampoline_Scheduler_new(SV* /*class*/, initial_capacity = 32)
        UV initial_capacity;
    CODE:
    {
        Async_Trampoline_Scheduler* scheduler =
                Async_Trampoline_Scheduler_new(aTHX_ initial_capacity);

        if (scheduler == NULL)
            croak("allocation failed");

        RETVAL = scheduler;
    }
    OUTPUT: RETVAL

void
Async_Trampoline_Scheduler_DESTROY(scheduler)
        Async_Trampoline_Scheduler* scheduler;
    CODE:
    {
        Async_Trampoline_Scheduler_unref(aTHX_ scheduler);
    }


void
Async_Trampoline_Scheduler_enqueue(scheduler, async, ...)
        Async_Trampoline_Scheduler* scheduler;
        Async* async;
    CODE:
    {
        bool ok = Async_Trampoline_Scheduler_enqueue_without_dependencies(
                aTHX_ scheduler, async);

        if (!ok)
            croak("could not enqueue value");

        for (IV i = 2; i < items; i++)
        {
            SV* dep_sv = ST(i);
            if (!sv_isa(dep_sv, "Async::Trampoline"))
                croak("Argument %d must be Async: ", i, SvPV_nolen(dep_sv));
            Async* dep = (Async*) SvIV(SvRV(dep_sv));

            Async_Trampoline_Scheduler_block_on(
                    aTHX_ scheduler, async, dep);
        }
    }

Async*
Async_Trampoline_Scheduler_dequeue(scheduler)
        Async_Trampoline_Scheduler* scheduler;
    CODE:
    {
        Async* async = Async_Trampoline_Scheduler_dequeue(aTHX_ scheduler);
        if (async == NULL)
            XSRETURN_EMPTY;

        RETVAL = async;
    }
    OUTPUT: RETVAL

void
Async_Trampoline_Scheduler_complete(scheduler, async)
        Async_Trampoline_Scheduler* scheduler;
        Async* async;
    CODE:
    {
        Async_Trampoline_Scheduler_complete(aTHX_ scheduler, async);
    }
