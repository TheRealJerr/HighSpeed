#ifndef TASK_HPP
#define TASK_HPP

#include <type_traits>
#include <coroutine>
#include <iostream> // 使用 iostream 代替 print
#include <thread>
#include <future>
#include <optional>
#include "Scheduler.hpp"

template <typename T>
struct Task
{
    struct promise_type
    {
        std::promise<T> value_;

        Task get_return_object() { 
            auto coro = std::coroutine_handle<promise_type>::from_promise(*this);
            Scheduler::get_instance().add_coroutine(coro);
            return Task(coro);
        }
        auto initial_suspend() { return std::suspend_always{}; }
        auto final_suspend() noexcept { return std::suspend_always{}; }
        void return_value(const T& value) { value_.set_value(value); }
        void unhandled_exception() { value_.set_exception(std::current_exception()); }
    };

    T get()
    {
        std::future<T> result = coro_.promise().value_.get_future();
        
        return result.get();
    }

    std::coroutine_handle<promise_type> coro_; // 协程句柄

    Task() : coro_(nullptr) {}
    Task(std::coroutine_handle<promise_type> coro) : coro_(coro) {}

    ~Task() {
        if (coro_) {
            coro_.destroy();
        }
    }
};
#endif