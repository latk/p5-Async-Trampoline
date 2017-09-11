#include "Scheduler.h"

#include "CircularBuffer.h"

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

// == The Impl Declaration ==

class Async_Trampoline_Scheduler::Impl {
    CircularBuffer<AsyncRef> runnable_queue{};
    std::unordered_set<Async const*> runnable_enqueued{};
    std::unordered_multimap<Async const*, AsyncRef> blocked{};

public:

    Impl(size_t initial_capacity);
    ~Impl();

    auto queue_size() const -> size_t { return runnable_queue.size(); }
    void enqueue(AsyncRef async);
    AsyncRef dequeue();
    void block_on(Async const& dependency_async, AsyncRef blocked_async);
    void complete(Async const& async);
};

// == The Public C++ Interface ==

Async_Trampoline_Scheduler::Async_Trampoline_Scheduler(size_t initial_capacity)
    : m_impl{new Async_Trampoline_Scheduler::Impl{initial_capacity}}
{}

Async_Trampoline_Scheduler::~Async_Trampoline_Scheduler() = default;

auto Async_Trampoline_Scheduler::queue_size() const -> size_t
{
    return m_impl->queue_size();
}

auto Async_Trampoline_Scheduler::enqueue(AsyncRef async) -> void
{
    m_impl->enqueue(std::move(async));
}

auto Async_Trampoline_Scheduler::dequeue() -> AsyncRef
{
    return m_impl->dequeue();
}

auto Async_Trampoline_Scheduler::block_on(
        Async const& dependency_async, AsyncRef blocked_async) -> void
{
    m_impl->block_on(dependency_async, std::move(blocked_async));
}

auto Async_Trampoline_Scheduler::complete(Async const& async) -> void
{
    m_impl->complete(async);
}

// == The Impl implementation ==

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

#define ASYNC_FORMAT "<Async 0x%zx %s>"
#define ASYNC_FORMAT_ARGS(async) (size_t) (async), Async_Type_name((async)->type)

Async_Trampoline_Scheduler::Impl::Impl(
        size_t initial_capacity)
{
    runnable_queue.grow(initial_capacity);
}

Async_Trampoline_Scheduler::Impl::~Impl()
{
    LOG_DEBUG(
            "clearing queue: " SCHEDULER_RUNNABLE_QUEUE_FORMAT "\n",
            SCHEDULER_RUNNABLE_QUEUE_FORMAT_ARGS(*this));
}

void Async_Trampoline_Scheduler::Impl::enqueue(AsyncRef async)
{
    LOG_DEBUG("enqueueing %p into " SCHEDULER_RUNNABLE_QUEUE_FORMAT ": " ASYNC_FORMAT "\n",
            async.decay(),
            SCHEDULER_RUNNABLE_QUEUE_FORMAT_ARGS(*this),
            ASYNC_FORMAT_ARGS(async));

    if (runnable_enqueued.find(async.decay()) != runnable_enqueued.end())
    {
        LOG_DEBUG("enqueuing skieeped because already in queue\n");
        return;
    }

    Async* key = async.decay();
    runnable_queue.enq(std::move(async));
    runnable_enqueued.insert(key);

    LOG_DEBUG(
            "    '-> " SCHEDULER_RUNNABLE_QUEUE_FORMAT "\n",
            SCHEDULER_RUNNABLE_QUEUE_FORMAT_ARGS(*this));
}

void Async_Trampoline_Scheduler::Impl::block_on(
        Async const& dependency_async, AsyncRef blocked_async)
{
    LOG_DEBUG(
        "dependency of " ASYNC_FORMAT " on " ASYNC_FORMAT "\n",
        ASYNC_FORMAT_ARGS(blocked_async.decay()),
        ASYNC_FORMAT_ARGS(&dependency_async));

    blocked.insert({&dependency_async, std::move(blocked_async)});
}

AsyncRef Async_Trampoline_Scheduler::Impl::dequeue()
{
    assert(runnable_queue.size());

    AsyncRef async = runnable_queue.deq();

    LOG_DEBUG(
            "dequeue %p from " SCHEDULER_RUNNABLE_QUEUE_FORMAT "\n",
            async,
            SCHEDULER_RUNNABLE_QUEUE_FORMAT_ARGS(*this));

    auto entry = runnable_enqueued.find(async.decay());
    if (entry == runnable_enqueued.end())
    {
        assert(0 /* dequeued an entry that was not registered in the enqueued set! */);
    }
    runnable_enqueued.erase(entry);

    return async;
}

void Async_Trampoline_Scheduler::Impl::complete(Async const& async)
{
    LOG_DEBUG("completing %p\n", &async);

    auto count = blocked.count(&async);
    LOG_DEBUG("    '-> %zu dependencies\n", count);

    if (count == 0)
        return;

    for (auto it = blocked.find(&async)
            ; it != blocked.end()
            ; it = blocked.find(&async))
    {
        enqueue(std::move(it->second));
        blocked.erase(it);
    }
}
