#ifndef STRAND_HPP
#define STRAND_HPP

#include <tools/ThreadPool.hpp>
#include <functional>
#include <mutex>
#include <queue>
#include <atomic>
#include <future>

namespace hspd
{

class Strand
{
public:
    using Executor_t = ThreadPool;
    using Task = std::function<void()>;

    explicit Strand(Executor_t* executor)
        : executor_(executor)
    {}

    Strand(const Strand&) = delete;
    Strand& operator=(const Strand&) = delete;
    Strand(Strand&&) = default;
    Strand& operator=(Strand&&) = default;

    template <class F, class... Args>
    auto addTask(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>;

private:
    void scheduleNext();

private:
    Executor_t* executor_;
    std::mutex mutex_;
    std::queue<Task> queue_;
    std::atomic_bool running_ = false;
};

template <class F, class... Args>
auto Strand::addTask(F&& f, Args&&... args)
    -> std::future<std::invoke_result_t<F, Args...>>
{
    using result_t = std::invoke_result_t<F, Args...>;

    auto task_ptr = std::make_shared<std::packaged_task<result_t()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    Task wrapped = [task_ptr]() { (*task_ptr)(); };

    bool needSchedule = false;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(wrapped));
        // 如果之前不在运行，则需要启动调度
        if (!running_.exchange(true)) {
            needSchedule = true;
        }
    }

    if (needSchedule) {
        scheduleNext();  // ⚠ 关键：不能在持锁状态调用
    }

    return task_ptr->get_future();
}

inline void Strand::scheduleNext()
{
    Task next;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (queue_.empty()) {
            running_ = false;
            return;
        }

        next = std::move(queue_.front());
        queue_.pop();
    }

    executor_->addTask([this, next = std::move(next)]() mutable {
        next();
        scheduleNext();  // worker 线程执行后继续调度
    });
}
// 将Strand和Executor_t分离，使得Strand可以被多个线程安全地使用

inline Strand makeStrand(Strand::Executor_t* executor)
{
    return Strand(executor);
}

} // namespace hspd

#endif
