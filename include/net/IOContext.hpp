
#ifndef IOCONTEXT_HPP
#define IOCONTEXT_HPP

#include <coroutine>
#include <atomic>
#include <unordered_map>
#include <functional>
#include <utility>
#include <stdexcept>

#include <sys/epoll.h>

#include <tools/ThreadPool.hpp>
#include <net/Epoll.hpp>
#include <log/Log.hpp>

namespace hspd {

class IOContext {
public:
    using Scheduler_t = ThreadPool;
    using EventLoop_t = Epoll;

    struct Waiter {
        std::coroutine_handle<> handle = nullptr;
        uint32_t events = 0;
    };

    IOContext(Scheduler_t* executor, EventLoop_t* ev)
        : executor_(executor), ev_(ev), running_(false)
    {
        if (!executor_) throw std::invalid_argument("IOContext needs a ThreadPool");
        if (!ev_) throw std::invalid_argument("IOContext needs an Epoll");
    }

    ~IOContext() = default;

    // expose executor for co_spawn to inject into top-level promise
    Scheduler_t* get_executor() const noexcept { return executor_; }

    // ----------------- await_fd: 返回用于 co_await 的 Awaiter --------------
    struct FdAwaiter {
        IOContext* ctx;
        int fd;
        uint32_t events;

        bool await_ready() const noexcept { return false; }

        // 当协程挂起时，保存句柄并注册等待（不动 promise->executor）
        void await_suspend(std::coroutine_handle<> awaiting) noexcept {
            // 保存 waiter（覆盖旧的 waiter）
            ctx->waiters_[fd] = Waiter{ awaiting, events };
            LOG_INFO("IOContext: fd {} registered for waiter {}", fd, awaiting.address());
            // 确保 epoll 上的关注事件与请求一致（add/modify）
            try {
                ctx->ev_->modify(fd, events);
            } catch (...) {
                // 如果 epoll 修改失败，不要抛异常到这里（await_suspend noexcept）
                // 将 waiter 移除，协程会陷入不可恢复状态，但我们尽量记录日志
                LOG_DEBUG("IOContext: epoll modify failed for fd {}", fd);
                try { ctx->waiters_.erase(fd); } catch (...) {}
            }
        }

        void await_resume() noexcept {}
    };

    FdAwaiter await_fd(int fd, uint32_t events) {
        return FdAwaiter{ this, fd, events };
    }

    // ----------------- co_spawn（将顶层 promise 的 executor 注入） -------------
    template <typename AwaitableT>
    void co_spawn(AwaitableT&& awaitable)
    {
        // 要求以 move 传入
        if (!awaitable.handle) {
            throw std::invalid_argument("co_spawn: null coroutine handle");
        }

        // 将 ThreadPool* 注入到最顶层 promise（你的 Awaitable 期望 ThreadPool*）
        awaitable.handle.promise().executor = this->executor_;

        // 取走句柄所有权（防止 awaitable 析构时 destroy）
        auto h = std::exchange(awaitable.handle,
                               std::coroutine_handle<typename std::decay_t<AwaitableT>::promise_type>{});

        // 将协程第一次调度到线程池（线程池会 resume() 协程）
        executor_->addTask([h]() mutable {
            h.resume();
        });
    }

    // ----------------- 事件循环 ----------------
    void run()
    {
        running_.store(true);
        // 启动线程池内部线程
        executor_->run();

        while (running_.load()) {
            int n = ev_->wait(events_, EVENTS_MAX, -1);
            if (n < 0) {
                // 允许被中断等，继续循环或记录
                continue;
            }
            Waiter waiter;
            for (int i = 0; i < n; ++i) {
                int fd = events_[i].data.fd;
                uint32_t ev = events_[i].events;

                std::lock_guard<std::mutex> lock(waitter_mtx_);
                // 不断获取Waiter放入线程池进行调度, dispatch到线程池
                {
                        // 找到对应 waiter，交给线程池去 resume()
                    auto it = waiters_.find(fd);
                    if (it == waiters_.end()) {
                        // 未找到 waiter：可能是race或已被移除，记录并 continue
                        // 可能存在空转的可能, 但不影响正确性, async_read并不是依赖于Epoll模型
                        // async_read首先在线程池中进行调度
                        // 1. 被调度的时候实现他会直接读取, 如果读取不成功, 他才会以来Epoll模型进行事件通知
                        continue;
                    }

                    waiter = it->second;
                    waiters_.erase(it);
                }

                // 派发到线程池恢复协程（resume 需要在 worker 线程执行）
                executor_->addTask([h = waiter.handle]() mutable {
                    // 恢复协程
                    h.resume();
                });
            }
        }
    }

    void stop()
    {
        running_.store(false);
        if (executor_) executor_->stop();
    }

    // helper: register a new fd initially (wrap epoll add)
    void add_fd(int fd, uint32_t events) {
        ev_->add(fd, events);
    }

    // helper: remove fd
    void remove_fd(int fd) {
        // 如果有 waiter，移除
        std::lock_guard<std::mutex> lock(waitter_mtx_);
        {
            waiters_.erase(fd);
        }
        ev_->remove(fd);
        LOG_INFO("IOContext: fd {} removed", fd);
    }

    void modify_fd(int fd, uint32_t events) {
        ev_->modify(fd, events);
        LOG_INFO("IOContext: fd {} modified", fd);
    }

    uint32_t get_events(int fd)
    {
        std::lock_guard<std::mutex> lock(waitter_mtx_);
        return waiters_[fd].events;
    }
private:
    std::mutex waitter_mtx_;
    static inline constexpr int EVENTS_MAX = 64;
    Scheduler_t* executor_;
    EventLoop_t* ev_;
    std::atomic_bool running_;
    std::unordered_map<int, Waiter> waiters_;
    struct epoll_event events_[EVENTS_MAX];

};

} // namespace hspd

#endif // IOCONTEXT_HPP
