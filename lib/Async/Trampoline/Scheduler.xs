#define PERL_NO_GET_CONTEXT

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "CircularBuffer.h"

#ifndef ASYNC_TRAMPOLINE_SCHEDULER_DEBUG
#define ASYNC_TRAMPOLINE_SCHEDULER_DEBUG 0
#else
#define ASYNC_TRAMPOLINE_SCHEDULER_DEBUG 1
#endif

typedef CIRCULAR_BUFFER(SV*) CircularBuffer_SVptr;

typedef struct {
    CircularBuffer_SVptr runnable_queue;
    HV* runnable_enqueued;
    HV* blocked;
} Async_Trampoline_Scheduler;

#define KEY_OF_REFERENCE(ref_sv) (newSVpvf("%lx", (long) SvRV(ref_sv)))

#define SCHEDULER_RUNNABLE_QUEUE_FORMAT                                     \
    "scheduler { "                                                          \
        "queue={ start=%ld size=%ld storage.size=%ld } "                    \
        "enqueued_hash=%ld "                                                \
        "blocked_on=%ld "                                                   \
    "}"

#define SCHEDULER_RUNNABLE_QUEUE_FORMAT_ARGS(sched)                         \
    (sched).runnable_queue.start,                                           \
    (sched).runnable_queue.size,                                            \
    (sched).runnable_queue.storage.size,                                    \
    HvFILL((sched).runnable_enqueued),                                      \
    HvFILL((sched).blocked)

#define ASYNC_FORMAT \
    "%s"

#define ASYNC_FORMAT_ARGS(async) \
    SvPV_nolen(async)

bool
Async_Trampoline_Scheduler_enqueue_without_dependencies(
        Async_Trampoline_Scheduler* scheduler, SV* async)
{
    // SV* key_sv = sv_2mortal(newSVnv(SvNV(async)));
    SV* key_sv = sv_2mortal(KEY_OF_REFERENCE(async));

    if (ASYNC_TRAMPOLINE_SCHEDULER_DEBUG)
        warn(
                "#DEBUG enqueueing key=%s into " SCHEDULER_RUNNABLE_QUEUE_FORMAT " : " ASYNC_FORMAT "\n",
                SvPV_nolen(key_sv),
                SCHEDULER_RUNNABLE_QUEUE_FORMAT_ARGS(*scheduler),
                ASYNC_FORMAT_ARGS(async));

    if (hv_exists_ent(scheduler->runnable_enqueued, key_sv, 0))
    {
        if (ASYNC_TRAMPOLINE_SCHEDULER_DEBUG)
            warn("#DEBUG enqueueing skipped because already in queue\n");
        return 1;
    }

    bool ok;
    CIRCULAR_BUFFER_ENQ(ok, SV*, scheduler->runnable_queue, SvREFCNT_inc(async));

    if (ok)
        hv_store_ent(scheduler->runnable_enqueued, key_sv, &PL_sv_undef, 0);

    if (ASYNC_TRAMPOLINE_SCHEDULER_DEBUG)
        warn("#DEBUG   '-> %s             " SCHEDULER_RUNNABLE_QUEUE_FORMAT "\n",
                (ok ? "succeeded" : "failed   "),
                SCHEDULER_RUNNABLE_QUEUE_FORMAT_ARGS(*scheduler));

    return ok;
}

MODULE = Async::Trampoline::Scheduler PACKAGE = Async::Trampoline::Scheduler PREFIX = Async_Trampoline_Scheduler_

Async_Trampoline_Scheduler*
Async_Trampoline_Scheduler_new(SV* /*class*/, initial_capacity = 32)
        UV initial_capacity;
    CODE:
    {
        Async_Trampoline_Scheduler* scheduler = NULL;
        Newxz(scheduler, 1, Async_Trampoline_Scheduler);

        scheduler->runnable_queue =
            (CircularBuffer_SVptr) CIRCULAR_BUFFER_INIT;

        bool ok;
        CIRCULAR_BUFFER_GROW(ok, SV*, scheduler->runnable_queue, initial_capacity);
        if (!ok)
        {
            CIRCULAR_BUFFER_FREE(scheduler->runnable_queue);
            Safefree(scheduler);
            croak("allocation failed");
        }

        scheduler->runnable_enqueued = newHV();

        scheduler->blocked = newHV();

        RETVAL = scheduler;
    }
    OUTPUT: RETVAL

void
Async_Trampoline_Scheduler_DESTROY(scheduler)
        Async_Trampoline_Scheduler* scheduler;
    CODE:
    {
        if (ASYNC_TRAMPOLINE_SCHEDULER_DEBUG)
        {
            fprintf(stderr, "#DEBUG clearing queue in DESTROY:  " SCHEDULER_RUNNABLE_QUEUE_FORMAT "\n",
                    SCHEDULER_RUNNABLE_QUEUE_FORMAT_ARGS(*scheduler));
            fflush(stderr);
        }

        while (CIRCULAR_BUFFER_SIZE(scheduler->runnable_queue))
        {
            SV* item;
            CIRCULAR_BUFFER_DEQ(item, scheduler->runnable_queue);
            SvREFCNT_dec(item);
        }
        CIRCULAR_BUFFER_FREE(scheduler->runnable_queue);

        SvREFCNT_dec(scheduler->runnable_enqueued);

        SvREFCNT_dec(scheduler->blocked);
    }


void
Async_Trampoline_Scheduler_enqueue(scheduler, async, ...)
        Async_Trampoline_Scheduler* scheduler;
        SV* async;
    CODE:
    {
        SV* key_sv = sv_2mortal(KEY_OF_REFERENCE(async));
        STRLEN key_len;
        const char* key = SvPV(key_sv, key_len);

        if (ASYNC_TRAMPOLINE_SCHEDULER_DEBUG)
            warn("#DEBUG deps for   key=%s into " SCHEDULER_RUNNABLE_QUEUE_FORMAT "\n",
                    SvPV_nolen(key_sv),
                    SCHEDULER_RUNNABLE_QUEUE_FORMAT_ARGS(*scheduler));

        IV required_args = 2;
        if (items > required_args)
        {
            SV** maybe_entry_sv =
                hv_fetch(scheduler->blocked, key, key_len, 1);
            if (maybe_entry_sv == NULL)
                croak("hash lookup failed");

            SV* entry_sv = *maybe_entry_sv;

            AV* blocked_list = NULL;
            if (SvROK(entry_sv) && SvTYPE(SvRV(entry_sv)) == SVt_PVAV)
            {
                blocked_list = (AV*) SvRV(entry_sv);
            }
            else
            {
                blocked_list = newAV();
                sv_setsv(entry_sv, newRV_noinc((SV*) blocked_list));
            }

            if (ASYNC_TRAMPOLINE_SCHEDULER_DEBUG)
                if (!SvROK(entry_sv))
                    croak("dependency hash entry could not be created!");

            for (IV i = required_args; i < items; i++)
            {
                if (ASYNC_TRAMPOLINE_SCHEDULER_DEBUG)
                    warn("#DEBUG dependency of " ASYNC_FORMAT " on " ASYNC_FORMAT "\n",
                            ASYNC_FORMAT_ARGS(ST(i)),
                            ASYNC_FORMAT_ARGS(async));
                av_push(blocked_list, SvREFCNT_inc(ST(i)));
            }
        }

        bool ok = Async_Trampoline_Scheduler_enqueue_without_dependencies(
                scheduler, async);

        if (!ok)
            croak("could not enqueue value");
    }

SV*
Async_Trampoline_Scheduler_dequeue(scheduler)
        Async_Trampoline_Scheduler* scheduler;
    CODE:
    {
        if (CIRCULAR_BUFFER_SIZE(scheduler->runnable_queue) == 0)
            XSRETURN_EMPTY;

        SV* async;
        CIRCULAR_BUFFER_DEQ(async, scheduler->runnable_queue);

        SV* key_sv = sv_2mortal(KEY_OF_REFERENCE(async));
        if (ASYNC_TRAMPOLINE_SCHEDULER_DEBUG)
            warn("#DEBUG dequeued   key=%s from " SCHEDULER_RUNNABLE_QUEUE_FORMAT "\n",
                    SvPV_nolen(key_sv),
                    SCHEDULER_RUNNABLE_QUEUE_FORMAT_ARGS(*scheduler));
        SV* entry_sv = hv_delete_ent(scheduler->runnable_enqueued, key_sv, 0, 0);
        if (entry_sv == NULL)
            croak("dequeued an entry that was not registered in the enqueued set! "
                    ASYNC_FORMAT,
                    ASYNC_FORMAT_ARGS(async));

        RETVAL = async;
    }
    OUTPUT: RETVAL

void
Async_Trampoline_Scheduler_complete(scheduler, async)
        Async_Trampoline_Scheduler* scheduler;
        SV* async;
    CODE:
    {
        SV* key_sv = sv_2mortal(KEY_OF_REFERENCE(async));
        if (ASYNC_TRAMPOLINE_SCHEDULER_DEBUG)
            warn("#DEBUG completing key=%s\n", SvPV_nolen(key_sv));
        SV* blocked_sv = hv_delete_ent(scheduler->blocked, key_sv, 0, 0);

        if (blocked_sv == NULL)
        {
            if (ASYNC_TRAMPOLINE_SCHEDULER_DEBUG)
                warn("#DEBUG   `-> no dependencies\n");
            XSRETURN_EMPTY;
        }

        if (ASYNC_TRAMPOLINE_SCHEDULER_DEBUG)
        {
            if (SvOK(blocked_sv))
                warn("#DEBUG   `-> sv=%s\n", SvPV_nolen(blocked_sv));
            else
                warn("#DEBUG   `-> sv=(undef)\n");
        }

        if (SvROK(blocked_sv) && SvTYPE(SvRV(blocked_sv)) == SVt_PVAV)
        {
            AV* blocked = (AV*) SvRV(blocked_sv);
            if (ASYNC_TRAMPOLINE_SCHEDULER_DEBUG)
                warn("#DEBUG  `-> %ld dependencies\n", av_len(blocked) + 1);
            bool ok = 1;
            while (ok && av_len(blocked) > -1)
            {
                SV* item = av_shift(blocked);
                ok = Async_Trampoline_Scheduler_enqueue_without_dependencies(
                        scheduler, item);
                SvREFCNT_dec(item);

            }
            if (!ok)
                croak("could not enqueue unblocked items");
        }
        else
        {
            croak("blocked entry was not array of asyncs");
        }
    }
