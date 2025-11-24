#ifndef GENERATOR_HPP
#define GENERATOR_HPP

#include <coroutine>
#include <iterator>
#include <stdexcept>
#include <exception>
#include <utility>
#include <any>
#include <optional>
template <typename T>
struct Generator
{
public:
    struct promise_type
    {
        std::optional<T> value_;
        std::exception_ptr exception_ = nullptr;

        Generator get_return_object()
        {
            return Generator{std::coroutine_handle<promise_type>::from_promise(*this)};
        }

        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }

        std::suspend_always yield_value(const T& value)
        {
            value_ = value;
            return {};
        }

        std::suspend_always yield_value(T&& value)
        {
            value_ = std::move(value);
            return {};
        }

        void return_void() {}

        void unhandled_exception()
        {
            exception_ = std::current_exception();
        }
    };

    using handle_t = std::coroutine_handle<promise_type>;
    
    Generator() noexcept = default;

    Generator(handle_t coro) : coro_(coro) {}

    // 禁止拷贝，允许移动
    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;
    
    Generator(Generator&& other) noexcept : coro_(std::exchange(other.coro_, {})) {}
    
    Generator& operator=(Generator&& other) noexcept
    {
        if (this != &other) {
            if (coro_) {
                coro_.destroy();
            }
            coro_ = std::exchange(other.coro_, {});
        }
        return *this;
    }

    ~Generator()
    {
        if (coro_) {
            coro_.destroy();
        }
    }

    // 迭代器类
    class iterator
    {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        handle_t coro_;
        bool is_end_ = false;

        iterator() noexcept : is_end_(true) {}
        explicit iterator(handle_t coro) noexcept : coro_(coro), is_end_(!coro || coro.done()) {}

        // 前置递增
        iterator& operator++()
        {
            if (is_end_ || !coro_ || coro_.done()) {
                is_end_ = true;
                return *this;
            }
            
            coro_.resume();
            
            if (coro_.done()) {
                is_end_ = true;
            } else if (coro_.promise().exception_) {
                std::rethrow_exception(coro_.promise().exception_);
            }
            
            return *this;
        }

        // 后置递增
        void operator++(int)
        {
            ++(*this);
        }

        // 解引用
        reference operator*() const
        {
            if (is_end_ || !coro_ || coro_.done()) {
                throw std::runtime_error("Cannot dereference ended iterator");
            }
            return coro_.promise().value_.value();
        }

        pointer operator->() const
        {
            return &**this;
        }

        // 比较操作
        bool operator==(const iterator& other) const noexcept
        {
            if (is_end_ && other.is_end_) return true;
            if (is_end_ != other.is_end_) return false;
            return coro_ == other.coro_;
        }

        bool operator!=(const iterator& other) const noexcept
        {
            return !(*this == other);
        }
    };

    // 获取开始迭代器
    iterator begin()
    {
        if (!coro_ || coro_.done()) {
            return iterator{};
        }
        
        // 创建迭代器，此时协程已经在初始挂起状态
        auto it = iterator{coro_};
        
        // 第一次解引用时应该得到第一个 yield 的值
        // 由于协程在 initial_suspend 时挂起，我们需要恢复它来获取第一个值
        if (!it.is_end_) {
            it.coro_.resume();
            if (it.coro_.done()) {
                it.is_end_ = true;
            } else if (it.coro_.promise().exception_) {
                std::rethrow_exception(it.coro_.promise().exception_);
            }
        }
        
        return it;
    }

    // 获取结束迭代器
    iterator end() noexcept
    {
        return iterator{};
    }

    // 便捷方法：手动迭代
    bool next()
    {
        if (!coro_ || coro_.done()) {
            return false;
        }
        
        coro_.resume();
        
        if (coro_.done()) {
            return false;
        }
        
        if (coro_.promise().exception_) {
            std::rethrow_exception(coro_.promise().exception_);
        }
        
        return true;
    }

    // 获取当前值
    T value() const
    {
        if (!coro_ || coro_.done()) {
            throw std::runtime_error("No value available");
        }
        return coro_.promise().value_;
    }

    // 检查是否有效
    explicit operator bool() const
    {
        return coro_ && !coro_.done();
    }

private:
    handle_t coro_ = nullptr;
};

#endif // GENERATOR_HPP