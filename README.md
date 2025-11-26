# High Speed

## **注意**

这个项目没有任何的依赖, 唯一的依赖就是C++20的编译器, 所以需要使用C++20的编译器才能编译通过。

## ✅ alloc/alloc.hpp : 内存池模块

效仿TcMalloc实现的简易的内存池, 但是没有TcMalloc分为三层的那么复杂, 思路大概一直, ThreadSafeMemoryPool是线程安全的线程私有内存池, 而GlobalMemoryPool是全局的内存池, 两者都实现了分配和释放内存的接口。

示例代码

```cpp
// 创建一个对象
Allocator::alloc<type>(args...);
Allocator::dealloc(type* ptr);

// 创建一个数组
Allocator::alloc_array<type>(size_t n, args...);
Allocator::dealloc_array(type* ptr, n);
```

## ✅ C++ 网络 IO Buffer：精炼接口（io/Buffer.h）

```cpp
#pragma once
#include <vector>
#include <string>
#include <cstddef>

class Buffer {
public:
    explicit Buffer(size_t initialSize = 1024);

    // -------------------------------
    //           基本属性
    // -------------------------------
    size_t readableBytes() const;      // 可读区域大小
    size_t writableBytes() const;      // 可写区域大小
    size_t prependableBytes() const;   // 可前置区域大小

    // -------------------------------
    //      读（不移动数据指针）
    // -------------------------------
    const char* peek() const;          // 可读数据起点
    const char* beginWriteConst() const;
    char* beginWrite();

    // -------------------------------
    //      消费数据（移动指针）
    // -------------------------------
    void retrieve(size_t len);         // 取走 len 字节
    void retrieveUntil(const char* end); // 取到某个位置
    void retrieveAll();                // 清空 Buffer
    std::string retrieveAsString(size_t len);  // 取字符串
    std::string retrieveAllAsString();

    // -------------------------------
    //      写入数据（自动扩容）
    // -------------------------------
    void append(const std::string& str);
    void append(const char* data, size_t len);
    void append(const void* data, size_t len);

    // -------------------------------
    //   网络 IO：读写 socket fd
    // -------------------------------
    ssize_t readFd(int fd, int* savedErrno);   // readv + 自动扩容
    ssize_t writeFd(int fd, int* savedErrno);  // write + 自动 retrieve

    // -------------------------------
    //      额外功能
    // -------------------------------
    void shrink(size_t reserve);       // 收缩缓冲区，节省内存
    void ensureWritableBytes(size_t len); // 保证可写空间

private:
    void makeSpace(size_t len);        // 扩容或移动数据

private:
    std::vector<char> buffer_;
    size_t readIndex_;
    size_t writeIndex_;

    static const size_t kCheapPrepend = 8;
};
```

---

## ✅ 每个接口的设计目的说明

---

### 📌 1. 基本属性

```cpp
size_t readableBytes() const;
size_t writableBytes() const;
size_t prependableBytes() const;
```

便于上层协议处理，比如 HTTP/Redis/TCP framing。

---

### 📌 2. peek / beginWrite

```cpp
const char* peek() const;
char* beginWrite();
```

返回 **读起点** 和 **写起点**，不移动指针，零拷贝。

---

### 📌 3. retrieve（消费数据）

```cpp
void retrieve(size_t len);
void retrieveUntil(const char* end);
void retrieveAll();
```

对协议解析特别重要：

* `retrieveUntil("\r\n")`
* `retrieve(4)` 读取固定长度包头
* `retrieveAll()` 清空

---

### 📌 4. append（写入数据）

```cpp
void append(const char* data, size_t len);
void append(const void* data, size_t len);
void append(const std::string& str);
```

自动扩容，适合构造发送缓冲区。

---

### 📌 5. readFd / writeFd

```cpp
ssize_t readFd(int fd, int* savedErrno);
ssize_t writeFd(int fd, int* savedErrno);
```

网络 IO 的核心：

* `readFd` 使用 `readv`，未读完的读入临时栈空间，减少系统调用
* 自动更新 writeIndex
* 自动扩容

---

### 📌 6. shrink

```cpp
void shrink(size_t reserve);
```

用于长连接防止内存无限增长：

```cpp
buffer.shrink(1024); // 留 1KB，其他缩掉
```

---



## ✅ 工具包tools

### 📌 tools/ThreadPools.hpp

1. 线程池类 `ThreadPool`
2. 线程池管理类 `ThreadPoolFactory`

```cpp
// 添加task任务
template <typename F, typename... Args>
auto addTask(F&& f, Args&&... args) -> std::future<typename std::invoke_result_t<F, Args...>>;

// 启动线程池
void run();

// 停止调度器
void stop();

// 获取任务数量
size_t getTaskCount() const { return task_num_.load(); }

// 获取等待线程数量
int getWaitingThreads() const { return waiting_threads_.load(); }
```

**实例代码**

```cpp
// 通过工厂创建线程池
auto pool = ThreadPoolFactory::createThreadPool();

auto task = [](int i)-> int
{
    std::cout << "Hello, ThreadPool!" << std::endl;
    return i;
};

auto ret = pool->addTask(task, 10);



// 启动
pool->run();

std::cout << "Result: " << ret.get() << std::endl; // 注意, run的时候才会创建线程, 因此promise::get()不能再run之前调用

std::this_thread::sleep_for(std::chrono::seconds(1));
```
**注意**

我们定义了全局的线程池`gThreadPool`, 因此在程序中只需要调用`gThreadPool->addTask`即可, 而不需要创建线程池对象。当然用户也可以自定义的创建自己的线程池。


## ✅ coro协程模块

### 📌 coro/Task.hpp 和 coro/Scheduler.hpp 和 coro/Generator.hpp

**Task** 

内部稳定绑定Schedular, 可以通过get方法获取到协程的返回值, 协程在创建的时候将任务分配给Scheduler, 实现了异步性, Task这里采用的是恶汉的思想, 创建即执行, 当我们get的时候, 数据可能早已完成等待我们的使用, Scheduler不参与生命周期的管理, 他只是协程句柄的调度者, 具体的协程句柄的声明周期的管理由用户决定。

**Scheduler**

针对线程池进行的协程句柄的特化, 专门的为Task设计的全局的线程池, Task创建的时候自动调用。

**Generator**

类似std::generator, 支持用户co_yeild, 但是不支持返回值, 用户层只能co_return无参数的返回



### 📌 coro/Awaitable.hpp

**Awaitable**
类似于Boost.Asio的awaitable, 用于协程的异步操作, 协程可以await一个awaitable对象, 等待其完成, 协程的异步操作可以被挂起, 切换到其他协程, 等到awaitable对象完成后再恢复, 实现了协程的异步操作, 对于这个Awaitable的对象, 我们需要将根的操作bind到一个exector上, 默认我们bind的是一个线程池, 也可以自定义一个executor, 用于协程的异步操作。

co_spawn使我们标准的绑定的接口, 它支持我们将协程对象直接传递给executor, 使得协程的异步操作可以被执行。

**示例代码**

```cpp
#include <coro/Awaitable.hpp>
#include <tools/ThreadPool.hpp>
Awaitable<int> test_1()
{
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    co_return 1;
}
Awaitable<void> test()
{
    int ret = co_await test_1();
    std::cout << "test_1 return " << ret << std::endl;
    co_return;
}
int main()
{
    assert(gThreadPool.get() != nullptr);
    gThreadPool->run();
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    co_spawn(*gThreadPool, test());
    
    // 等待一秒
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    return 0;
}

``` 

## ✅ 网络板块

### 📌 net/Socket.hpp

**Socket**
```cpp
class Socket
{   
    // 异步的读写接口
    Awaitable<size_t> async_read(Buffer& buffer);
    Awaitable<size_t> async_write(Buffer& buffer);
};

```cpp
class Acceptor
{
    // 异步的accept接口
    Awaitable<Socket> async_accept();
};
```
### 📌 net/Epoll.hpp

提供Epoll的各种接口, 包括:
```cpp
void add(int fd, uint32_t events);
void modify(int fd, uint32_t events);
void remove(int fd);
int wait(struct epoll_event* events, int max_events, int timeout);
```

**示例代码**

```cpp
// 测试网络板块
#include <net/Socket.hpp>
#include <tools/ThreadPool.hpp>
#include <iostream>

Epoll* gEpoll = new Epoll();

Awaitable<void> test_socket(Socket socket) {
    Buffer buffer;
    size_t n = co_await socket.async_read(buffer);
    std::cout << "read " << n << " bytes" << std::endl;
    n = co_await socket.async_write(buffer);
    std::cout << "write " << n << " bytes" << std::endl;
    socket.close();
}

Awaitable<void> test_server() {
    Acceptor acceptor(gThreadPool.get(), EndPoint{"0.0.0.0", 8080}, gEpoll);
    struct epoll_event events[10];

    while (true) {
        int nfds = gEpoll->wait(events, 10, -1);
        for (int i = 0; i < nfds; ++i) {
            if (events[i].data.fd == acceptor.fd()) {
                Socket socket = co_await acceptor.async_accept();
                if (socket.fd() >= 0) {
                    co_spawn(*gThreadPool, test_socket(std::move(socket)));
                }
            } else {
                // 处理已连接的套接字事件
                // 这里可以添加逻辑来处理具体的读写事件
            }
        }
    }
}

int main() {

    gThreadPool->run();    
    co_spawn(*gThreadPool, test_server());

    std::cin.get();
    return 0;
}

```
