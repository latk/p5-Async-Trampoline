#include "Scheduler.h"

#include "DynamicArray.h"
#include "CircularBuffer.h"

#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#ifndef ASYNC_TRAMPOLINE_SCHEDULER_DEBUG
#define ASYNC_TRAMPOLINE_SCHEDULER_DEBUG 0
#define LOG_DEBUG(...) do { } while (0)
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

struct Async_Trampoline_Scheduler {
    size_t refcount;
    CircularBuffer<Async*> runnable_queue;
    std::unordered_set<Async*> runnable_enqueued;
    std::unordered_multimap<Async*, Async*> blocked;

    Async_Trampoline_Scheduler(size_t initial_capacity);
    ~Async_Trampoline_Scheduler();

    void ref();
    void unref();

    void enqueue(Async* async);
    Async* dequeue();
    void block_on(Async* dependency_async, Async* blocked_async);
    void complete(Async* async);
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
    (self).runnable_enqueued.size(),                                        \
    (self).blocked.size()

Async_Trampoline_Scheduler*
Async_Trampoline_Scheduler_new(
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
    runnable_enqueued{},
    blocked{}
{
    runnable_queue.grow(initial_capacity);
}

void Async_Trampoline_Scheduler::ref()
{
    refcount++;
}

void
Async_Trampoline_Scheduler_ref(
        Async_Trampoline_Scheduler* self)
{
    assert(self != NULL);
    self->ref();
}

void Async_Trampoline_Scheduler::unref()
{
    refcount--;

    if (refcount == 0)
        delete this;
}

void
Async_Trampoline_Scheduler_unref(
        Async_Trampoline_Scheduler* self)
{
    assert(self != NULL);
    self->unref();
}

Async_Trampoline_Scheduler::~Async_Trampoline_Scheduler()
{
    LOG_DEBUG(
            "clearing queue: " SCHEDULER_RUNNABLE_QUEUE_FORMAT "\n",
            SCHEDULER_RUNNABLE_QUEUE_FORMAT_ARGS(*this));

    // clear out remaining enqueued items
    while (runnable_queue.size())
    {
        Async* item = runnable_queue.deq();
        Async_unref(item);
    }

    // clear out blocked items
    for (auto it = blocked.begin(); it != blocked.end(); ++it)
    {
        Async_unref(it->second);
        blocked.erase(it);
    }
}

void Async_Trampoline_Scheduler::enqueue(Async* async)
{
    LOG_DEBUG("enqueueing %p into " SCHEDULER_RUNNABLE_QUEUE_FORMAT ": " ASYNC_FORMAT "\n",
            async,
            SCHEDULER_RUNNABLE_QUEUE_FORMAT_ARGS(*this),
            ASYNC_FORMAT_ARGS(async));

    if (runnable_enqueued.find(async) != runnable_enqueued.end())
    {
        LOG_DEBUG("enqueuing skieeped because already in queue\n");
        return;
    }

    runnable_queue.enq(async);

    Async_ref(async);
    runnable_enqueued.insert(async);

    LOG_DEBUG(
            "    '-> " SCHEDULER_RUNNABLE_QUEUE_FORMAT "\n",
            SCHEDULER_RUNNABLE_QUEUE_FORMAT_ARGS(*this));
}

bool
Async_Trampoline_Scheduler_enqueue_without_dependencies(
        Async_Trampoline_Scheduler* self,
        Async* async)
{
    assert(self);
    assert(async);

    try
    {
        self->enqueue(async);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void Async_Trampoline_Scheduler::block_on(
        Async* dependency_async, Async* blocked_async)
{
    LOG_DEBUG(
        "dependency of " ASYNC_FORMAT " on " ASYNC_FORMAT "\n",
        ASYNC_FORMAT_ARGS(blocked_async),
        ASYNC_FORMAT_ARGS(dependency_async));

    blocked.insert({dependency_async, blocked_async});
    Async_ref(blocked_async);
}

void
Async_Trampoline_Scheduler_block_on(
        Async_Trampoline_Scheduler* self,
        Async* dependency_async,
        Async* blocked_async)
{
    assert(self);
    assert(dependency_async);
    assert(blocked_async);

    self->block_on(dependency_async, blocked_async);
}

Async* Async_Trampoline_Scheduler::dequeue()
{
    assert(runnable_queue.size());

    Async* async = runnable_queue.deq();

    LOG_DEBUG(
            "dequeue %p from " SCHEDULER_RUNNABLE_QUEUE_FORMAT "\n",
            async,
            SCHEDULER_RUNNABLE_QUEUE_FORMAT_ARGS(*this));

    auto entry = runnable_enqueued.find(async);
    if (entry == runnable_enqueued.end())
    {
        assert(0 /* dequeued an entry that was not registered in the enqueued set! */);
    }
    runnable_enqueued.erase(entry);

    return async;
}

Async*
Async_Trampoline_Scheduler_dequeue(
    Async_Trampoline_Scheduler* self)
{
    if (self->runnable_queue.size() == 0)
        return NULL;

    return self->dequeue();
}

void Async_Trampoline_Scheduler::complete(Async* async)
{
    LOG_DEBUG("completing %p\n", async);

    auto count = blocked.count(async);
    LOG_DEBUG("    '-> %zu dependencies\n", count);

    if (count == 0)
        return;

    for (auto it = blocked.find(async)
            ; it != blocked.end()
            ; it = blocked.find(async))
    {
        enqueue(it->second);
        blocked.erase(it);
    }
}

void
Async_Trampoline_Scheduler_complete(
        Async_Trampoline_Scheduler* self,
        Async* async)
{
    assert(self);
    assert(async);

    self->complete(async);
}
