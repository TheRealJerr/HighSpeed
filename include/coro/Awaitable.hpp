#pragma once

#include <coroutine>
#include <optional>
#include <exception>
#include <future>
#include <type_traits>
#include <utility>      // std::exchange
#include <tools/ThreadPool.hpp>

namespace hspd
{
    // ======================= Awaitable<T> =======================
    template <typename T>
    struct Awaitable {
        struct promise_type {
            ThreadPool* executor = nullptr;
            std::coroutine_handle<> awaiting;
            std::optional<T> value;
            std::exception_ptr exception;

            Awaitable get_return_object() {
                return Awaitable{
                    std::coroutine_handle<promise_type>::from_promise(*this)
                };
            }

            std::suspend_always initial_suspend() noexcept { return {}; }

            struct final_awaiter {
                bool await_ready() noexcept { return false; }
                void await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                    auto& p = h.promise();
                    if (p.awaiting) {
                        p.awaiting.resume();
                    }
                }
                void await_resume() noexcept {}
            };

            final_awaiter final_suspend() noexcept { return {}; }

            void return_value(T v) { value = std::move(v); }
            void unhandled_exception() { exception = std::current_exception(); }

            template <typename U>
            auto await_transform(Awaitable<U> a) {
                a.handle.promise().executor = this->executor;
                return a;
            }

            template <typename Awaiter>
            decltype(auto) await_transform(Awaiter&& a) noexcept {
                return std::forward<Awaiter>(a);
            }
        };

        std::coroutine_handle<promise_type> handle{};

        // ctor from handle
        explicit Awaitable(std::coroutine_handle<promise_type> h) : handle(h) {
            if (!handle) throw std::invalid_argument("Coroutine handle is null");
        }

        // delete copy semantics
        Awaitable(const Awaitable&) = delete;
        Awaitable& operator=(const Awaitable&) = delete;

        // provide move semantics
        Awaitable(Awaitable&& other) noexcept : handle(other.handle) {
            other.handle = {};
        }
        Awaitable& operator=(Awaitable&& other) noexcept {
            if (this != &other) {
                // 销毁自己持有的（如果有）
                if (handle) handle.destroy();
                handle = other.handle;
                other.handle = {};
            }
            return *this;
        }

        bool await_ready() noexcept { return false; }

        void await_suspend(std::coroutine_handle<> h) {
            auto& p = handle.promise();
            assert(p.executor);
            p.awaiting = h;
            p.executor->addTask([handle = this->handle]() mutable {
                handle.resume();
            });
        }

        T await_resume() {
            auto& p = handle.promise();
            if (p.exception) std::rethrow_exception(p.exception);
            return std::move(p.value.value());
        }

        ~Awaitable() {
            if (handle) handle.destroy();
        }
    };

    // ======================= Awaitable<void> =======================
    template <>
    struct Awaitable<void> {
        struct promise_type {
            ThreadPool* executor = nullptr;
            std::coroutine_handle<> awaiting;
            std::exception_ptr exception;

            Awaitable get_return_object() {
                return Awaitable{
                    std::coroutine_handle<promise_type>::from_promise(*this)
                };
            }

            std::suspend_always initial_suspend() noexcept { return {}; }

            struct final_awaiter {
                bool await_ready() noexcept { return false; }
                void await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                    auto& p = h.promise();
                    if (p.awaiting) p.awaiting.resume();
                }
                void await_resume() noexcept {}
            };

            final_awaiter final_suspend() noexcept { return {}; }

            void return_void() {}
            void unhandled_exception() { exception = std::current_exception(); }

            auto await_transform(Awaitable<void> a) {
                a.handle.promise().executor = this->executor;
                return a;
            }

            template <typename U>
            auto await_transform(Awaitable<U> a) {
                a.handle.promise().executor = this->executor;
                return a;
            }

            template <typename Awaiter>
            decltype(auto) await_transform(Awaiter&& a) noexcept {
                return std::forward<Awaiter>(a);
            }
        };

        std::coroutine_handle<promise_type> handle{};

        explicit Awaitable(std::coroutine_handle<promise_type> h) : handle(h) {
            if (!handle) throw std::invalid_argument("Coroutine handle is null");
        }

        Awaitable(const Awaitable&) = delete;
        Awaitable& operator=(const Awaitable&) = delete;

        Awaitable(Awaitable&& other) noexcept : handle(other.handle) {
            other.handle = {};
        }
        Awaitable& operator=(Awaitable&& other) noexcept {
            if (this != &other) {
                if (handle) handle.destroy();
                handle = other.handle;
                other.handle = {};
            }
            return *this;
        }

        bool await_ready() noexcept { return false; }

        void await_suspend(std::coroutine_handle<> h) {
            auto& p = handle.promise();
            assert(p.executor);
            p.awaiting = h;
            p.executor->addTask([handle = this->handle]() mutable {
                handle.resume();
            });
        }

        void await_resume() {
            auto& p = handle.promise();
            if (p.exception) std::rethrow_exception(p.exception);
        }

        ~Awaitable() {
            if (handle) handle.destroy();
        }
    };

    // ======================= co_spawn =======================
    template <typename AwaitableT>
    void co_spawn(ThreadPool& pool, AwaitableT&& awaitable)
    {
        // 强制以 move 的语义传入（调用者请使用 std::move(a)）
        if (!awaitable.handle) {
            throw std::invalid_argument("Coroutine handle is null");
        }

        // 注入 executor 到最顶层 promise
        awaitable.handle.promise().executor = &pool;

        // 取出句柄的所有权，确保 awaitable 的析构不会 destroy()
        auto h = std::exchange(awaitable.handle, std::coroutine_handle<typename std::decay_t<AwaitableT>::promise_type>{});

        pool.addTask([h]() mutable {
            h.resume();
        });
    }



}
