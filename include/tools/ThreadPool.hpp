#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

// 这是一个支持future返回值的线程池

#include <queue>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cassert>
#include <functional>
#include <future>
#include <type_traits>
#include <memory>
#include <alloc/alloc.hpp>


namespace hspd {
class ThreadPool;
}

namespace std {
    template <>
    struct default_delete<hspd::ThreadPool> {
        void operator()(hspd::ThreadPool* ptr) const;
    };
}

namespace hspd {

class ThreadPool
{
    using task_t = std::function<void()>;
    
    // 线程的执行函数
    void worker();

    void _addTask(task_t task);

public:
    // 构造函数
    ThreadPool() = default;

    // 添加task任务
    template <typename F, typename... Args>
    auto addTask(F&& f, Args&&... args) -> std::future<typename std::invoke_result_t<F, Args...>>;

    // 启动线程池
    void run();

    // 停止调度器，等待所有任务完成
    void stop();
    
    // 强制停止调度器，不等待任务完成
    void stopHard();
    
    // 获取任务数量
    size_t getTaskCount() const { return task_num_.load(); }
    
    // 获取等待线程数量
    int getWaitingThreads() const { return waiting_threads_.load(); }
    
    // 析构函数
    ~ThreadPool() {
        if (running_.load()) {
            stop();
        }
    }
    
private:
    // 锁
    mutable std::mutex mtx_; 
    // 条件变量
    std::condition_variable cv_;
    // 任务队列
    std::queue<task_t> task_queue_;
    // 定义线程池
    std::vector<std::thread> threads_;
    // 等待线程的数量
    std::atomic<int> waiting_threads_ = 0;
    // 调度器的状态
    std::atomic<bool> running_ = false;
    // 当前任务的个数
    std::atomic<int> task_num_ = 0;
    // 定义线程的多少
    inline static int THREADS_NUM = 2;
};

// 添加线程池的任务
template <typename F, typename... Args>
auto ThreadPool::addTask(F&& f, Args&&... args) -> std::future<typename std::invoke_result_t<F, Args...>>
{
    using result_t = typename std::invoke_result_t<F, Args...>;
    
    // 使用 packaged_task 统一处理，避免直接操作 promise
    auto task_ptr = std::make_shared<std::packaged_task<result_t()>>(
        [func = std::forward<F>(f), 
         tuple_args = std::make_tuple(std::forward<Args>(args)...)]() mutable {
            return std::apply(func, std::move(tuple_args));
        }
    );
    
    // 创建可拷贝的lambda包装器
    auto wrapper = [task_ptr]() {
        (*task_ptr)();
    };
    
    _addTask(std::move(wrapper));
    return task_ptr->get_future();
}

void ThreadPool::_addTask(task_t task)
{
    assert(task);
    {
        std::unique_lock<std::mutex> lock(mtx_);
        task_queue_.push(std::move(task));
        task_num_++;
    }
    
    // 通知等待的线程任务到来
    if (waiting_threads_ > 0) {
        cv_.notify_one();
    }
}

void ThreadPool::run()
{
    if (running_.exchange(true)) {
        std::cout << "ThreadPool is already running" << std::endl;
        return;
    }
    
    // 创建线程池
    std::cout << "Starting ThreadPool with " << THREADS_NUM << " threads" << std::endl;
    for (size_t i = 0; i < THREADS_NUM; ++i) {
        threads_.emplace_back([this]() {
            this->worker();
        });
    }
}

void ThreadPool::worker() {
    std::cout << "Worker thread " << std::this_thread::get_id() << " started" << std::endl;
    
    while (true) {
        task_t task;
        {
            std::unique_lock<std::mutex> lock(mtx_);
            
            // 等待条件：停止运行或有任务可执行
            waiting_threads_++;
            cv_.wait(lock, [this]() {
                return !running_.load() || !task_queue_.empty();
            });
            waiting_threads_--;
            
            // 检查退出条件
            if (!running_.load() && task_queue_.empty()) {
                std::cout << "Worker thread " << std::this_thread::get_id() << " exiting" << std::endl;
                return;
            }
            
            // 取出任务
            if (!task_queue_.empty()) {
                task = std::move(task_queue_.front());
                task_queue_.pop();
                task_num_--;
            }
        }
        
        // 执行任务
        if (task) {
            try {
                task();
            } catch (const std::exception& e) {
                std::cerr << "Exception in worker thread: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "Unknown exception in worker thread" << std::endl;
            }
        }
    }
}

// 停止调度器，等待所有任务完成
void ThreadPool::stop() {
    if (!running_.exchange(false)) {
        std::cout << "ThreadPool is already stopped" << std::endl;
        return;
    }
    
    std::cout << "Stopping ThreadPool..." << std::endl;
    
    // 通知所有等待的线程
    cv_.notify_all();
    
    // 等待线程退出，回收所有的线程
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    threads_.clear();
    
    // 清空剩余任务（可选）
    {
        std::unique_lock<std::mutex> lock(mtx_);
        while (!task_queue_.empty()) {
            task_queue_.pop();
            task_num_--;
        }
    }
    
    std::cout << "ThreadPool stopped successfully" << std::endl;
}

// 强制停止调度器，不等待任务完成
void ThreadPool::stopHard() {
    if (!running_.exchange(false)) {
        std::cout << "ThreadPool is already stopped" << std::endl;
        return;
    }
    
    std::cout << "Stopping ThreadPool forcefully..." << std::endl;
    
    // 通知所有等待的线程
    cv_.notify_all();
    
    // 等待线程退出，回收所有的线程
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.detach(); // 强制退出线程，不等待任务完成
        }
    }
    
    threads_.clear();
    
    std::cout << "ThreadPool stopped forcefully" << std::endl;
}

// 定制化析构函数


// 通过工厂模式创建一个线程池
using ThreadPoolPtr = std::shared_ptr<ThreadPool>;

class ThreadPoolFactory
{
public:
    static ThreadPoolPtr createThreadPool() {
        return std::make_shared<ThreadPool>();
    }
};

// 定义一个默认的全局的线程池
inline static auto gThreadPool = ThreadPoolFactory::createThreadPool();

} // namespace hspd

void std::default_delete<hspd::ThreadPool>::operator()(hspd::ThreadPool* ptr) const {
    ptr->stop();
    delete ptr;
}

#endif
