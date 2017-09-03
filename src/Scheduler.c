#include "Scheduler.h"

#include "CircularBuffer.h"

#ifndef ASYNC_TRAMPOLINE_SCHEDULER_DEBUG
#define ASYNC_TRAMPOLINE_SCHEDULER_DEBUG 0
#define LOG_DEBUG(pattern, ...) do { } while (0)
#else
#define ASYNC_TRAMPOLINE_SCHEDULER_DEBUG 1
#define LOG_DEBUG(...) do {                                                 \
    fprintf(stderr, "#DEBUG " __VA_ARGS__);                                 \
    fflush(stderr);                                                         \
} while (0)
#endif

typedef SV Async;

#define ASYNC_FORMAT "%s"
#define ASYNC_FORMAT_ARGS(async) SvPV_nolen(async)

static
void
Async_ref(pTHX_ Async* async)
{
    SvREFCNT_inc(async);
}

static
void
Async_unref(pTHX_ Async* async)
{
    SvREFCNT_dec(async);
}

static
SV*
Async_key(pTHX_ Async* async)
{
    return newSVpvf("0x%zx", (size_t) SvIV(SvRV(async)));
    // if in the future custom hash tables are implemented,
    // consider <https://stackoverflow.com/a/12996028>
    // or <http://www.isthe.com/chongo/tech/comp/fnv/>
    // for the hash function.
}

typedef CIRCULAR_BUFFER(Async*) CircularBuffer_Async;

struct Async_Trampoline_Scheduler {
    size_t refcount;
    CircularBuffer_Async runnable_queue;
    HV* runnable_enqueued;
    HV* blocked;
};

#define SCHEDULER_RUNNABLE_QUEUE_FORMAT                                     \
    "Scheduler { "                                                          \
        "queue={ start=%zu size=%ld storage.size=%ld } "                    \
        "runnable_enqueued=%zu "                                            \
        "blocked_on=%zu "                                                   \
    "}"

#define SCHEDULER_RUNNABLE_QUEUE_FORMAT_ARGS(self)                          \
    (self).runnable_queue.start,                                            \
    (self).runnable_queue.size,                                             \
    (self).runnable_queue.storage.size,                                     \
    HvFILL((self).runnable_enqueued),                                       \
    HvFILL((self).blocked)

Async_Trampoline_Scheduler*
Async_Trampoline_Scheduler_new(
        pTHX_
        size_t initial_capacity)
{
    Async_Trampoline_Scheduler* self =
        malloc(sizeof(Async_Trampoline_Scheduler));

    if (self == NULL)
        return NULL;

    self->refcount = 1;

    self->runnable_queue = (CircularBuffer_Async) CIRCULAR_BUFFER_INIT;

    bool ok;
    CIRCULAR_BUFFER_GROW(ok, Async*, self->runnable_queue, initial_capacity);
    if (!ok)
    {
        CIRCULAR_BUFFER_FREE(self->runnable_queue);
        free(self);
        return NULL;
    }

    self->runnable_enqueued = newHV();

    self->blocked = newHV();

    return self;
}

void
Async_Trampoline_Scheduler_ref(
        Async_Trampoline_Scheduler* self)
{
    assert(self != NULL);

    self->refcount++;
}

void
Async_Trampoline_Scheduler_unref(
        pTHX_
        Async_Trampoline_Scheduler* self)
{
    assert(self != NULL);

    self->refcount--;

    if (self->refcount != 0)
        return;

    LOG_DEBUG(
            "clearing queue: " SCHEDULER_RUNNABLE_QUEUE_FORMAT "\n",
            SCHEDULER_RUNNABLE_QUEUE_FORMAT_ARGS(*self));

    while (CIRCULAR_BUFFER_SIZE(self->runnable_queue))
    {
        Async* item;
        CIRCULAR_BUFFER_DEQ(item, self->runnable_queue);
        Async_unref(item);
    }
    CIRCULAR_BUFFER_FREE(self->runnable_queue);

    SvREFCNT_dec(self->runnable_enqueued);

    SvREFCNT_dec(self->blocked);
}

bool
Async_Trampoline_Scheduler_enqueue_without_dependencies(
        pTHX_
        Async_Trampoline_Scheduler* self,
        Async* async)
{
    SV* key_sv = sv_2mortal(Async_key(aTHX_ async));

    LOG_DEBUG("enqueueing key=%s into " SCHEDULER_RUNNABLE_QUEUE_FORMAT ": " ASYNC_FORMAT "\n",
            SvPV_nolen(key_sv),
            SCHEDULER_RUNNABLE_QUEUE_FORMAT_ARGS(*self),
            ASYNC_FORMAT_ARGS(async));

    if (hv_exists_ent(self->runnable_enqueued, key_sv, 0))
    {
        LOG_DEBUG("enqueueing skipped because already in queue\n");
        return true;
    }

    bool ok;
    CIRCULAR_BUFFER_ENQ(ok, Async*, self->runnable_queue, (Async_ref(async), async));
    if (ok)
        hv_store_ent(self->runnable_enqueued, key_sv, &PL_sv_undef, 0);

    LOG_DEBUG(
            "    '-> %s " SCHEDULER_RUNNABLE_QUEUE_FORMAT "\n",
            (ok ? "succeeded" : "failed"),
            SCHEDULER_RUNNABLE_QUEUE_FORMAT_ARGS(*self));

    return ok;
}

void
Async_Trampoline_Scheduler_block_on(
        pTHX_
        Async_Trampoline_Scheduler* self,
        Async* dependency_async,
        Async* blocked_async)
{
    SV* key_sv = sv_2mortal(Async_key(dependency_async));
    STRLEN key_len;
    const char* key = SvPV(key_sv, key_len);

    SV** maybe_entry_sv = hv_fetch(self->blocked, key, key_len, 1);
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

    if (!SvROK(entry_sv))
        croak("dependency hash entry could not be created!");

    LOG_DEBUG(
        "dependency of " ASYNC_FORMAT " on " ASYNC_FORMAT "\n",
        ASYNC_FORMAT_ARGS(blocked_async),
        ASYNC_FORMAT_ARGS(dependency_async));

    Async_ref(blocked_async);
    av_push(blocked_list, blocked_async);
}

Async*
Async_Trampoline_Scheduler_dequeue(
    pTHX_
    Async_Trampoline_Scheduler* self)
{
    if (CIRCULAR_BUFFER_SIZE(self->runnable_queue) == 0)
        return NULL;

    Async* async;
    CIRCULAR_BUFFER_DEQ(async, self->runnable_queue);

    SV* key_sv = sv_2mortal(Async_key(async));

    LOG_DEBUG(
            "dequeued key=%s from " SCHEDULER_RUNNABLE_QUEUE_FORMAT "\n",
            SvPV_nolen(key_sv),
            SCHEDULER_RUNNABLE_QUEUE_FORMAT_ARGS(*self));

    SV* entry_sv = hv_delete_ent(self->runnable_enqueued, key_sv, 0, 0);
    if (entry_sv == NULL)
        croak("dequeued an entry that was not registered in the enqueued set! " ASYNC_FORMAT,
                ASYNC_FORMAT_ARGS(async));

    return async;
}

void
Async_Trampoline_Scheduler_complete(
        pTHX_
        Async_Trampoline_Scheduler* self,
        Async* async)
{
    SV* key_sv = sv_2mortal(Async_key(async));

    LOG_DEBUG("completing key=%s\n", SvPV_nolen(key_sv));

    SV* blocked_sv = hv_delete_ent(self->blocked, key_sv, 0, 0);

    if (blocked_sv == NULL)
    {
        LOG_DEBUG("    '-> no dependencies\n");
        return;
    }

    if (SvOK(blocked_sv))
        LOG_DEBUG("    '-> sv=%s\n", SvPV_nolen(blocked_sv));
    else
        LOG_DEBUG("    '-> sv=(undef)\n");

    if (!SvROK(blocked_sv) || SvTYPE(SvRV(blocked_sv)) != SVt_PVAV)
        croak("blocked entry was not an array of asyncs");

    AV* blocked = (AV*) SvRV(blocked_sv);

    LOG_DEBUG("    '-> %zd dependencies\n", av_len(blocked) + 1);

    bool ok = true;

    while (ok && av_len(blocked) > -1)
    {
        Async* item = av_shift(blocked);
        ok = Async_Trampoline_Scheduler_enqueue_without_dependencies(
                aTHX_ self, item);
        Async_unref(item);
    }

    if (!ok)
        croak("could not unblock items");
}
