#pragma once
#include <cstdlib>
#include <new>
#include <mutex>
#include <vector>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <exception>

// ==============================================================
//                    ThreadSafeMemoryPool
//        固定块尺寸 + 多线程安全 + thread_local 缓存
// ==============================================================

class ThreadSafeMemoryPool {
public:
    ThreadSafeMemoryPool(size_t blockSize, size_t blocksPerChunk = 1024, size_t localCacheLimit = 64)
        : blockSize_(alignBlockSize(blockSize)),
          blocksPerChunk_(blocksPerChunk),
          localCacheLimit_(localCacheLimit)
    {
        assert(blockSize_ >= sizeof(void*) && "blockSize must be >= pointer size");
    }

    ~ThreadSafeMemoryPool() {
        for (void* c : chunks_) {
            ::operator delete(c);
        }
    }

    void* allocate() {
        auto &local = localCache();
        if (!local.empty()) {
            void* p = local.back();
            local.pop_back();
            return p;
        }

        refillLocalFromGlobal(local);

        if (!local.empty()) {
            void* p = local.back();
            local.pop_back();
            return p;
        }

        allocateChunkToLocal(local);
        void* p = local.back();
        local.pop_back();
        return p;
    }

    void deallocate(void* p) {
        if (!p) return;
        auto &local = localCache();
        local.push_back(p);

        if (local.size() > localCacheLimit_ * 2) {
            flushLocalToGlobal(local);
        }
    }

private:
    static size_t alignBlockSize(size_t s) {
        constexpr size_t align = alignof(std::max_align_t);
        return ((s + align - 1) / align) * align;
    }

    static std::vector<void*>& localCache() {
        thread_local std::vector<void*> cache;
        return cache;
    }

    void allocateChunkToLocal(std::vector<void*>& local) {
        size_t chunkBytes = blockSize_ * blocksPerChunk_;
        void* chunk = ::operator new(chunkBytes);

        {
            std::lock_guard<std::mutex> lock(chunksMutex_);
            chunks_.push_back(chunk);
        }

        for (size_t i = 0; i < blocksPerChunk_; i++) {
            void* block = static_cast<char*>(chunk) + i * blockSize_;
            local.push_back(block);
        }
    }

    void refillLocalFromGlobal(std::vector<void*>& local) {
        std::lock_guard<std::mutex> lock(globalMutex_);
        size_t toMove = std::min(localCacheLimit_, globalFreeList_.size());
        for (size_t i = 0; i < toMove; i++) {
            local.push_back(globalFreeList_.back());
            globalFreeList_.pop_back();
        }
    }

    void flushLocalToGlobal(std::vector<void*>& local) {
        std::lock_guard<std::mutex> lock(globalMutex_);
        size_t keep = localCacheLimit_;
        while (local.size() > keep) {
            globalFreeList_.push_back(local.back());
            local.pop_back();
        }
    }

private:
    const size_t blockSize_;
    const size_t blocksPerChunk_;
    const size_t localCacheLimit_;

    std::vector<void*> globalFreeList_;
    std::mutex globalMutex_;

    std::vector<void*> chunks_;
    std::mutex chunksMutex_;
};

// ==============================================================
//                     GlobalPoolManager
//          多尺寸池：8,16,24,...4096 字节 size-classes
// ==============================================================

static constexpr size_t ALIGN = 8;
static constexpr size_t MAX_POOL_SIZE = 4096;
static constexpr int NUM_CLASSES = MAX_POOL_SIZE / ALIGN;

static inline size_t align_up(size_t n) {
    return (n + ALIGN - 1) & ~(ALIGN - 1);
}

static inline int size_to_class(size_t n) {
    n = align_up(n);
    if (n > MAX_POOL_SIZE) return -1;
    return (int)(n / ALIGN) - 1;
}

class GlobalPoolManager {
public:
    GlobalPoolManager() {
        for (int i = 0; i < NUM_CLASSES; i++) {
            pools_[i] = new ThreadSafeMemoryPool((i + 1) * ALIGN, 1024, 64);
        }
    }

    ~GlobalPoolManager() {
        for (int i = 0; i < NUM_CLASSES; i++) {
            delete pools_[i];
        }
    }

    void* allocate(size_t size) {
        int cls = size_to_class(size);
        if (cls < 0) return std::malloc(size);
        return pools_[cls]->allocate();
    }

    void deallocate(void* p, size_t size) {
        int cls = size_to_class(size);
        if (cls < 0) {
            std::free(p);
            return;
        }
        pools_[cls]->deallocate(p);
    }

private:
    ThreadSafeMemoryPool* pools_[NUM_CLASSES];
};

static GlobalPoolManager gMemoryPool;

