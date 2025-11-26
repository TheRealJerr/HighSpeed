#ifndef ALLOC_HPP
#define ALLOC_HPP
#include "MemoryPool.hpp"

class Allocator
{
public:
    // 1. 普通对象分配
    template <typename T, typename... Args>
    static auto alloc(Args&&... args) -> T*
    {
        size_t sz = sizeof(T);
        void* p = gMemoryPool.allocate(sz);
        try {
            // 定位new
            return new (p) T(std::forward<Args>(args)...);
        } catch (...) {
            gMemoryPool.deallocate(p, sz);
            throw;
        }
    }

    // 2. 数组分配 - 使用重载而不是特化
    template <typename T>
    static auto alloc_array(size_t n) -> T*
    {
        size_t sz = sizeof(T) * n;
        void* p = gMemoryPool.allocate(sz);
        try {
            T* arr = static_cast<T*>(p);
            // 对每个元素进行构造
            for (size_t i = 0; i < n; ++i) {
                new (&arr[i]) T();  // 默认构造
            }
            return arr;
        } catch (...) {
            // 如果构造失败，释放已构造的对象和内存
            T* arr = static_cast<T*>(p);
            for (size_t i = 0; i < n; ++i) {
                arr[i].~T();
            }
            gMemoryPool.deallocate(p, sz);
            throw;
        }
    }

    // 带参数的数组分配版本
    template <typename T, typename... Args>
    static auto alloc_array(size_t n, Args&&... args) -> T*
    {
        size_t sz = sizeof(T) * n;
        void* p = gMemoryPool.allocate(sz);
        try {
            T* arr = static_cast<T*>(p);
            // 对每个元素使用相同的参数进行构造
            for (size_t i = 0; i < n; ++i) {
                new (&arr[i]) T(std::forward<Args>(args)...);
            }
            return arr;
        } catch (...) {
            // 异常安全处理
            T* arr = static_cast<T*>(p);
            for (size_t i = 0; i < n; ++i) {
                arr[i].~T();
            }
            gMemoryPool.deallocate(p, sz);
            throw;
        }
    }

    // 释放普通对象
    template <typename T>
    static void dealloc(T* p)
    {
        if (p) {
            p->~T();
            gMemoryPool.deallocate(p, sizeof(T));
        }
    }

    // 释放数组 - 使用不同的函数名避免歧义
    template <typename T>
    static void dealloc_array(T* p, size_t n)
    {
        if (p) {
            // 调用每个元素的析构函数
            for (size_t i = 0; i < n; ++i) {
                p[i].~T();
            }
            // 释放内存
            gMemoryPool.deallocate(p, sizeof(T) * n);
        }
    }
};

#endif