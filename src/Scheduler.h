#pragma once

#include "Async.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Async_Trampoline_Scheduler Async_Trampoline_Scheduler;

/** Create a new Scheduler.
 *
 *  initial_capacity: size_t
 *      should be power of 2.
 *  returns: Async_Trampoline_Scheduler* NULLABLE
 */
Async_Trampoline_Scheduler*
Async_Trampoline_Scheduler_new(
        size_t initial_capacity);

/** Create a new Scheduler with default capacity.
 *
 *  returns: Async_Trampoline_Scheduler*
 */
inline
Async_Trampoline_Scheduler*
Async_Trampoline_Scheduler_new_with_default_capacity()
{
    return Async_Trampoline_Scheduler_new(32);
}

/** Increment recount.
 *
 *  self: Async_Trampoline_Scheduler*
 */
void
Async_Trampoline_Scheduler_ref(
        Async_Trampoline_Scheduler* self);

/** Decrement refcount, possibly destroying the object.
 *
 *  If the queue still contains items, these will be freed.
 *
 *  self: Async_Trampoline_Scheduler*
 */
void
Async_Trampoline_Scheduler_unref(
        Async_Trampoline_Scheduler* self);

/** Enqueue an item as possibly runnable.
 *
 *  self: Async_Trampoline_Scheduler*
 *  async: Async*
 *  return: bool
 *      true if the item is enqueued, false if growing the buffer failed.
 *      NB that the item is not enqueued again if it is already enqueued.
 */
bool
Async_Trampoline_Scheduler_enqueue_without_dependencies(
        Async_Trampoline_Scheduler* self,
        Async* async);

/** Register a dependency relationship.
 *
 *  This is similar to a Makefile rule:
 *
 *      blocked_async: dependency_async
 *
 *  self: Async_Trampoline_Scheduler*
 *  dependency_async: Async*
 *      an Async that must complete first.
 *  blocked_async: Async*
 *      an Async that is blocked until the "dependency_async" is completed.
 */
void
Async_Trampoline_Scheduler_block_on(
        Async_Trampoline_Scheduler* self,
        Async* dependency_async,
        Async* blocked_async);

Async*
Async_Trampoline_Scheduler_dequeue(
        Async_Trampoline_Scheduler* self);

void
Async_Trampoline_Scheduler_complete(
        Async_Trampoline_Scheduler* self,
        Async* async);

#ifdef __cplusplus
}
#endif
