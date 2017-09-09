#pragma once

#include <cassert>
#include <memory>

struct Destructible_Vtable {
    using DestroyT = void (*)(void* data);
    using CopyT = void* (*)(void* data);

    DestroyT destroy;
    CopyT copy;

    Destructible_Vtable(DestroyT destroy, CopyT copy) :
        destroy{destroy},
        copy{copy}
    {
        assert(destroy);
        assert(copy);
    }
};

struct Destructible {
    void*                       data;
    Destructible_Vtable const*  vtable;

    Destructible(void* data, Destructible_Vtable const* vtable) :
        data{data},
        vtable{vtable}
    {
        assert(data);
        assert(vtable);
    }

    Destructible(Destructible&& other) = default;

    Destructible(Destructible const& other) :
        Destructible{other.vtable->copy(other.data), other.vtable}
    {}

    auto clear() -> void
    {
        if (vtable)
            vtable->destroy(data);
        data = nullptr;
        vtable = nullptr;
    }

    ~Destructible()
    {
        clear();
    }

    auto swap(Destructible& other) -> void
    {
        using std::swap;
        swap(data, other.data);
        swap(vtable, other.vtable);
    }

    auto operator= (Destructible const& other) -> Destructible&
    {
        clear();
        Destructible copy = other;
        swap(copy);
        return *this;
    }

    auto operator= (Destructible&& other) -> Destructible&
    {
        clear();
        swap(other);
        return *this;
    }
};

struct DestructibleTuple {
    Destructible_Vtable const* vtable;
    size_t size;
    void* data[1];

    static auto create(
            Destructible_Vtable const* vtable,
            size_t size)
        -> DestructibleTuple*
    {
        size_t alloc_size = sizeof(DestructibleTuple) - 1;
        alloc_size += size * sizeof(void*);

        char* storage = new char[alloc_size];
        return new (storage) DestructibleTuple{ vtable, size };
    }

    auto clear() -> void
    {
        this->~DestructibleTuple();
    }

    auto copy() const -> DestructibleTuple*
    {
        DestructibleTuple* the_copy = create(vtable, size);
        for (size_t i = 0; i < size; i++)
            the_copy->data[i] = vtable->copy(data[i]);

        return the_copy;
    }

    auto at(size_t i) -> void*
    {
        assert(i < size);
        return data[i];
    }

    auto copy_from(size_t i) -> Destructible
    {
        return { vtable->copy(at(i)), vtable };
    }

    auto move_from(size_t i) -> Destructible
    {
        assert(i < size);
        Destructible result { nullptr, vtable };
        using std::swap;
        swap(result.data, data[i]);
        return result;
    }

    auto copy_into(size_t i, Destructible const& source) -> void
    {
        assert(vtable == source.vtable);
        assert(i < size);
        assert(data[i] == nullptr);

        data[i] = vtable->copy(source.data);
    }

    auto move_into(size_t i, Destructible&& source) -> void
    {
        assert(vtable == source.vtable);
        assert(i < size);
        assert(data[i] == nullptr);

        using std::swap;
        swap(data[i], source.data);
        source.vtable = nullptr;
    }

private:
    DestructibleTuple(Destructible_Vtable const* vtable, size_t size) :
        vtable{vtable},
        size{size}
    {
        assert(vtable);
        for (size_t i = 0; i < size; i++)
            data[i] = nullptr;
    }

    ~DestructibleTuple()
    {
        for (size_t i = 0; i < size; i++)
        {
            vtable->destroy(data[i]);
            data[i] = nullptr;
        }
        delete[] reinterpret_cast<char*>(this);
    }

};
