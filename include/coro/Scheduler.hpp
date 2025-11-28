// Scheduler.hpp
#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

// 这个Scheduler类作用是协程调度器，它维护一个协程队列，并提供一个run()方法来执行协程队列中的协程。
#include <queue>
#include <coroutine>
#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cassert>
// attentions:
// 1. 协程的调度器不管理协程的创建和销毁，它只负责协程的调度，协程的创建和销毁由用户自己管理。
// 2. 协程的调度器是一个单例模式, 可以修改他的线程数字



const int THREADS_NUM = std::thread::hardware_concurrency();

class Scheduler;

namespace std
{
    template <>
    struct default_delete<Scheduler> {
        void operator()(Scheduler* ptr) const;
    };
}

class Scheduler
{
    // 线程的执行函数
    void worker();

    // 构造函数
    Scheduler() = default;

    
public:
    static Scheduler& get_instance()
    {
        if(!instance_)
        {
            instance_.reset(new Scheduler());
            instance_->run();
        }
        return *instance_;
    }

    // 定义协程句柄的类型
    using handle_t = std::coroutine_handle<>;

    // 添加协程的任务
    void add_coroutine(handle_t coro);

    // 启动调度器
    void run();

    // 停止调度器
    void stop();
    
private:
    // 锁
    std::mutex mtx_; 
    // 条件变量
    std::condition_variable cv_;
    // 协程队列
    std::queue<handle_t> corotines_;
    // 定义线程池
    std::vector<std::thread> threads_;
    // 等待线程的数量
    std::atomic<int> waiting_threads_ = 0;
    // 调度器的状态
    std::atomic<bool> running_ = false;
    // 当前任务的个数
    std::atomic<int> task_num_ = 0;

    // 单例模式
    inline static std::unique_ptr<Scheduler> instance_ = nullptr;
};

// 添加协程的任务
void Scheduler::add_coroutine(handle_t coro)
{
    assert(coro != nullptr && running_.load() == true);
    {
        std::unique_lock<std::mutex> lock(mtx_);
        corotines_.push(coro);
    }
    // 通知等待的线程任务到来
    if(waiting_threads_ > 0)
        cv_.notify_one();
    // 增加任务的个数
    task_num_++;
}

void Scheduler::run()
{
    // 创建线程池
    running_ = true;
    for(size_t i = 0; i < THREADS_NUM; ++i)
    {
        threads_.emplace_back([this](){
            this->worker();
        });
    }
}

// // 定义线程的任务函数
// void Scheduler::worker()
// {
//     // 运行中或者存在任务就会一直执行
//     while(running_.load() || task_num_ > 0)
//     {
//         handle_t coro;
//         {
//             std::unique_lock<std::mutex> lock(mtx_);
//             // 等待任务到来
//             if(running_ == false && task_num_ == 0) return;
//             while(task_num_ == 0)
//             {
//                 waiting_threads_++;
//                 cv_.wait(lock);
//                 waiting_threads_--;
//             }
//             if(task_num_ == 0 && running_ == false) return;
//             // 取出任务
//             coro = corotines_.front();
//             corotines_.pop();
//             task_num_--;
//         }
//         // 执行协程
//         coro.resume();
//     }
// }

void Scheduler::worker() {
    std::cout << "Worker thread started" << std::endl;
    while (running_.load() || task_num_ > 0) {
        handle_t coro;
        {
            std::unique_lock<std::mutex> lock(mtx_);
            // 等待任务到来
            while (task_num_ == 0 && running_.load()) {
                waiting_threads_++;
                cv_.wait(lock);
                waiting_threads_--;
            }
            // 如果调度器停止且任务队列为空，退出
            if (task_num_ == 0 && !running_.load()) {
                std::cout << "Worker thread exiting" << std::endl;
                return;
            }
            // 取出任务
            assert(!corotines_.empty());
            coro = corotines_.front();
            corotines_.pop();
            task_num_--;
        }
        // 执行协程
        std::cout << "Resuming coroutine" << std::endl;
        coro.resume();
    }
    std::cout << "Worker thread finished" << std::endl;
}


// 停止调度器
void Scheduler::stop() {
    running_ = false;
    // 通知等待的线程任务到来
    cv_.notify_all();
    // 等待线程退出, 回收所有的线程
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    // 判断任务执行完成
    assert(task_num_ == 0);
    std::cout << "Scheduler stopped" << std::endl;
}

void std::default_delete<Scheduler>::operator()(Scheduler* ptr) const
{
    ptr->stop();
    delete ptr;
}

#endif