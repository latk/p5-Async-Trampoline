#include "Scheduler.h"

#include "DynamicArray.h"
#include "CircularBuffer.h"

#include <stdexcept>

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

typedef DYNAMIC_ARRAY(Async*) AsyncList;

#define ASYNC_FORMAT "<Async 0x%zx %s>"
#define ASYNC_FORMAT_ARGS(async) (size_t) (async), Async_Type_name((async)->type)

static
SV*
Async_key(pTHX_ Async* async)
{
    return newSVpvf("0x%zx", (size_t) async);
    // if in the future custom hash tables are implemented,
    // consider <https://stackoverflow.com/a/12996028>
    // or <http://www.isthe.com/chongo/tech/comp/fnv/>
    // for the hash function.
}

struct Async_Trampoline_Scheduler {
    size_t refcount;
    CircularBuffer<Async*> runnable_queue;
    HV* runnable_enqueued;
    HV* blocked;

    Async_Trampoline_Scheduler(size_t initial_capacity);
    ~Async_Trampoline_Scheduler();
};

#define SCHEDULER_RUNNABLE_QUEUE_FORMAT                                     \
    "Scheduler { "                                                          \
        "queue={ start=%zu size=%ld storage.size=%ld } "                    \
        "runnable_enqueued=%zu "                                            \
        "blocked_on=%zu "                                                   \
    "}"

#define SCHEDULER_RUNNABLE_QUEUE_FORMAT_ARGS(self)                          \
    (self).runnable_queue._internal_start(),                                \
    (self).runnable_queue.size(),                                           \
    (self).runnable_queue.capacity(),                                       \
    HvFILL((self).runnable_enqueued),                                       \
    HvFILL((self).blocked)

Async_Trampoline_Scheduler*
Async_Trampoline_Scheduler_new(
        pTHX_
        size_t initial_capacity)
{
    try
    {
        return new Async_Trampoline_Scheduler{initial_capacity};
    }
    catch (...)
    {
        return nullptr;
    }
}

Async_Trampoline_Scheduler::Async_Trampoline_Scheduler(
        size_t initial_capacity) :
    refcount{0},
    runnable_queue{},
    runnable_enqueued{newHV()},
    blocked{newHV()}
{
    bool ok;
    runnable_queue.grow(ok, initial_capacity);
    if (!ok)
    {
        throw std::runtime_error("growing circular buffer");
    }
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

    delete self;
}

Async_Trampoline_Scheduler::~Async_Trampoline_Scheduler()
{
    LOG_DEBUG(
            "clearing queue: " SCHEDULER_RUNNABLE_QUEUE_FORMAT "\n",
            SCHEDULER_RUNNABLE_QUEUE_FORMAT_ARGS(*this));

    while (runnable_queue.size())
    {
        Async* item = runnable_queue.deq();
        Async_unref(item);
    }

    SvREFCNT_dec(this->runnable_enqueued);

    SvREFCNT_dec(this->blocked);
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
    self->runnable_queue.enq_from_cb(ok, [&] { Async_ref(async); return async; });

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

    AsyncList* blocked_list = NULL;
    if (SvOK(entry_sv)
            && SvIOK(entry_sv)
            && (blocked_list = (AsyncList*) SvIV(entry_sv)))
    {
        // ok
    }
    else
    {
        blocked_list = (AsyncList*) malloc(sizeof(AsyncList));
        *blocked_list = (AsyncList) DYNAMIC_ARRAY_INIT;
        sv_setiv(entry_sv, (IV) blocked_list);
    }

    assert(SvIOK(entry_sv));

    LOG_DEBUG(
        "dependency of " ASYNC_FORMAT " on " ASYNC_FORMAT "\n",
        ASYNC_FORMAT_ARGS(blocked_async),
        ASYNC_FORMAT_ARGS(dependency_async));

    bool ok;
    DYNAMIC_ARRAY_PUSH(
            ok,
            Async*,
            *blocked_list,
            (Async_ref(blocked_async), blocked_async));

    if (!ok)
        croak("block list allocation failed");
}

Async*
Async_Trampoline_Scheduler_dequeue(
    pTHX_
    Async_Trampoline_Scheduler* self)
{
    if (self->runnable_queue.size() == 0)
        return NULL;

    Async* async = self->runnable_queue.deq();

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

    AsyncList* blocked = NULL;
    if (SvIOK(blocked_sv)
            && (blocked = (AsyncList*) SvIV(blocked_sv)))
    {
        // ok
    }
    else
    {
        croak("blocked entry was not an array of asyncs");
    }

    LOG_DEBUG("    '-> %zd dependencies\n", DYNAMIC_ARRAY_SIZE(*blocked));

    bool ok = true;
    Async** item_ptr = NULL;
    for (size_t i = 0
            ; ok && (item_ptr = DYNAMIC_ARRAY_GETPTR(*blocked, i))
            ; i++)
    {
        ok = Async_Trampoline_Scheduler_enqueue_without_dependencies(
                aTHX_ self, *item_ptr);
    }

    if (!ok)
        croak("could not unblock items");

    while (DYNAMIC_ARRAY_SIZE(*blocked))
    {
        Async* item;
        DYNAMIC_ARRAY_POP(item, Async*, *blocked);
        Async_unref(item);
    }
}
