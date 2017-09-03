#pragma once

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#define MAKE_DESTRUCTIBLE(vtable_ptr, data) \
    ((Destructible) { (void*) data, vtable_ptr })

typedef struct Destructible_Vtable {
    void (*destroy)(void *data);
    void* (*copy)(void* source);
} const Destructible_Vtable;

typedef struct Destructible {
    void* data;
    Destructible_Vtable* vtable;
} Destructible;

static inline
void
Destructible_init(
       Destructible*           self,
        void*                   data,
        Destructible_Vtable*    vtable)
{
    assert(self);
    assert(self->data == NULL);
    assert(self->vtable == NULL);

    assert(vtable);
    assert(vtable->destroy);
    assert(vtable->copy);

    self->data = data;
    self->vtable = vtable;
}

static inline
void
Destructible_init_move(
        Destructible*   self,
        Destructible*   source)
{
    Destructible_init(self, source->data, source->vtable);
    source->data = NULL;
    source->vtable = NULL;
}

static inline
void
Destructible_init_copy(
        Destructible*   self,
        Destructible    source)
{
    assert(self);
    assert(source.vtable);

    void* data_copy = source.vtable->copy(source.data);
    Destructible_init(self, data_copy, source.vtable);
}

static inline
void
Destructible_clear(
        Destructible*   self)
{
    assert(self);
    assert(self->vtable);
    assert(self->vtable->destroy);

    self->vtable->destroy(self->data);
}

typedef struct DestructibleTuple {
    Destructible_Vtable* vtable;
    size_t size;
    void* data[];
} DestructibleTuple;

static inline
DestructibleTuple*
DestructibleTuple_new(
        Destructible_Vtable*    vtable,
        size_t                  size)
{
    assert(vtable);
    assert(vtable->copy);
    assert(vtable->destroy);

    DestructibleTuple* self = malloc(
            sizeof(DestructibleTuple) + size * sizeof(void*));

    if (self == NULL)
        return NULL;

    self->vtable = vtable;
    self->size = size;
    for (size_t i = 0; i < size; i++)
        self->data[i] = NULL;

    return self;
}

static inline
void
DestructibleTuple_clear(
        DestructibleTuple*  self)
{
    assert(self);

    Destructible_Vtable* vtable = self->vtable;
    assert(vtable);
    assert(vtable->destroy);

    for (size_t i = 0; i < self->size; i++)
    {
        vtable->destroy(self->data[i]);
        self->data[i] = NULL;
    }

    free(self);
}

static inline
DestructibleTuple*
DestructibleTuple_copy(
        DestructibleTuple*  orig)
{
    assert(orig);

    size_t size = orig->size;

    Destructible_Vtable* vtable = orig->vtable;
    assert(vtable);

    DestructibleTuple* self = malloc(
            sizeof(DestructibleTuple) + size * sizeof(void*));

    if (self == NULL)
        return NULL;

    self->vtable = vtable;
    self->size = size;
    for (size_t i = 0; i < size; i++)
        self->data[i] = vtable->copy(orig->data[i]);

    return self;
}

static inline
void*
DestructibleTuple_at(
        DestructibleTuple*  self,
        size_t              i)
{
    assert(self);
    assert(i < self->size);
    return self->data[i];
}

static inline
Destructible
DestructibleTuple_copy_from(
        DestructibleTuple*  self,
        size_t              i)
{
    assert(self);
    assert(self->vtable);
    assert(self->vtable->copy);

    void* orig = DestructibleTuple_at(self, i);
    void* copy = self->vtable->copy(orig);

    Destructible target = { .data = NULL, .vtable = NULL };
    Destructible_init(&target, copy, self->vtable);
    return target;
}

static inline
void
DestructibleTuple_copy_into(
        DestructibleTuple*  self,
        size_t              i,
        Destructible        source)
{
    assert(self);
    assert(self->vtable == source.vtable);
    assert(i < self->size);
    assert(self->data[i] == NULL);

    self->data[i] = self->vtable->copy(source.data);
}

static inline
Destructible
DestructibleTuple_move_from(
        DestructibleTuple*  self,
        size_t              i)
{
    assert(self);

    Destructible target;
    target.vtable = self->vtable;
    target.data = DestructibleTuple_at(self, i);
    self->data[i] = NULL;

    return target;
}

static inline
void
DestructibleTuple_move_into(
        DestructibleTuple*  self,
        size_t              i,
        Destructible*       source)
{
    assert(self);
    assert(source);
    assert(self->vtable == source->vtable);
    assert(i < self->size);
    assert(self->data[i] == NULL);

    Destructible temp = { NULL, NULL };
    Destructible_init_move(&temp, source);
    self->data[i] = temp.data;
}
