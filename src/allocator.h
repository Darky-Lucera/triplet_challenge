#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <scoped_allocator>
#include <type_traits>

#define kMax(a, b)  a > b ? a : b

//-------------------------------------
struct CustomPool {
    explicit CustomPool(size_t bufferSize)
        : mIndex(0)
        , mOffset(0)
        , mBufferSize(bufferSize) {
        mBuffers.emplace_back(reinterpret_cast<uint8_t *>(malloc(bufferSize)));
    }

    ~CustomPool() {
        for(uint8_t *ptr : mBuffers) {
            free(ptr);
        }
        mBuffers.clear();
    }

    void *
    allocate(size_t bufferSize, size_t align) {
        mOffset = (mOffset + align - 1) / align * align;
        if (mBuffers.empty() || mOffset + bufferSize >= mBufferSize) {
            mBuffers.emplace_back(reinterpret_cast<uint8_t *>(malloc(kMax(mBufferSize, bufferSize))));
            mIndex = mBuffers.size() - 1;
            mOffset = 0;
        }

        void *ptr = mBuffers[mIndex] + mOffset;
        mOffset += bufferSize;
        return ptr;
    }

    void 
    deallocate(void *ptr, size_t bufferSize) {
        // NO
    }

    CustomPool(CustomPool const &)            = delete;
    CustomPool &operator=(CustomPool const &) = delete;

protected:
    std::vector<uint8_t *>  mBuffers;
    size_t                  mIndex{};
    size_t                  mOffset{};
    size_t                  mBufferSize;
};

//-------------------------------------
template <typename T>
struct CustomAllocator {
    template <typename U> 
    friend struct CustomAllocator;

    using value_type      = T;
    using pointer         = T *;
    using reference       = T &;
    using const_reference = const T &;

    using propagate_on_container_copy_assignment = std::true_type;
    using propagate_on_container_move_assignment = std::true_type;
    using propagate_on_container_swap            = std::true_type;

    explicit CustomAllocator(CustomPool *a) : pool(a) { }

    template <typename U>
    CustomAllocator(CustomAllocator<U> const &rhs) : pool(rhs.pool) { }

    pointer allocate(size_t blocks) {
        return static_cast<pointer>(pool->allocate(blocks * sizeof(T), alignof(T)));
    }

    void deallocate(pointer ptr, size_t blocks) {
        pool->deallocate(ptr, blocks * sizeof(T));
    }

    template <typename U>
    bool operator==(CustomAllocator<U> const &rhs) const {
        return pool == rhs.pool;
    }

    template <typename U>
    bool operator!=(CustomAllocator<U> const &rhs) const {
        return pool != rhs.pool;
    }

protected:
    CustomPool  *pool;
};
