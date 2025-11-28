
// #ifndef SOCKET_HPP
// #define SOCKET_HPP

// #include <tools/ThreadPool.hpp>
// #include <Coro/Awaitable.hpp>
// #include <io/Buffer.hpp>
// #include <netinet/in.h>
// #include <sys/socket.h>
// #include <sys/types.h>
// #include <unistd.h>
// #include <string_view>
// #include <fcntl.h>
// #include <netdb.h>
// #include <arpa/inet.h>
// #include <sys/epoll.h>
// #include <iostream>
// #include <net/Epoll.hpp>

// namespace hspd
// {
//     struct EndPoint {
//         EndPoint(std::string_view ip, uint16_t port) : ip(ip), port(port) {}
//         std::string_view ip;
//         uint16_t port;
//     };

//     // 声明Socket
//     class Socket;


//     class Acceptor {
//     public:
//         using executor_t = ThreadPool;

//         // 没有默认的构造函数
//         Acceptor(executor_t* executor, EndPoint endpoint, Epoll* epoll);

//         Acceptor(const Acceptor&) = delete;
//         Acceptor& operator=(const Acceptor&) = delete;

//         Acceptor(Acceptor&& other) noexcept
//             : listenfd_(other.listenfd_), executor_(other.executor_), endpoint_(other.endpoint_), epoll_(other.epoll_) {
//             other.listenfd_ = -1;
//             other.executor_ = nullptr;
//             other.epoll_ = nullptr;
//         }
//         int fd() const noexcept {
//             return listenfd_;
//         }
//         Acceptor& operator=(Acceptor&& other) noexcept {
//             if (this != &other) {
//                 listenfd_ = other.listenfd_;
//                 executor_ = other.executor_;
//                 endpoint_ = other.endpoint_;
//                 epoll_ = other.epoll_;
//                 other.listenfd_ = -1;
//                 other.executor_ = nullptr;
//                 other.epoll_ = nullptr;
//             }
//             return *this;
//         }

//         // 标准的接口
//         Awaitable<Socket> async_accept();

//         void close();

//     private:
//         int listenfd_;
//         executor_t* executor_;
//         EndPoint endpoint_;
//         Epoll* epoll_;
//     };

//     class Socket {
//     public:
//         using executor_t = ThreadPool;

//         Socket() = default;
//         Socket(int sockfd, executor_t* executor, Epoll* epoll) : sockfd_(sockfd), executor_(executor), epoll_(epoll) {
//             if (sockfd_ >= 0) {
//                 epoll_->add(sockfd_, EPOLLIN | EPOLLOUT);
//             }
//         }

//         // 支持移动语义
//         Socket(const Socket&) = delete;
//         Socket& operator=(const Socket&) = delete;

//         Socket(Socket&& other) noexcept
//             : sockfd_(other.sockfd_), executor_(other.executor_), epoll_(other.epoll_) {
//             other.sockfd_ = -1;
//             other.executor_ = nullptr;
//             other.epoll_ = nullptr;
//         }

//         Socket& operator=(Socket&& other) noexcept {
//             if (this != &other) {
//                 sockfd_ = other.sockfd_;
//                 executor_ = other.executor_;
//                 epoll_ = other.epoll_;
//                 other.sockfd_ = -1;
//                 other.executor_ = nullptr;
//                 other.epoll_ = nullptr;
//             }
//             return *this;
//         }

//         int fd() const noexcept {
//             return sockfd_;
//         }

//         Awaitable<size_t> async_read(Buffer& buffer);
//         Awaitable<size_t> async_write(Buffer& buffer);

//         void close();

//     private:
//         int sockfd_ = -1;
//         executor_t* executor_ = nullptr;
//         Epoll* epoll_ = nullptr;
//     };

//     Acceptor::Acceptor(executor_t* executor, EndPoint endpoint, Epoll* epoll)
//         : listenfd_(-1), executor_(executor), endpoint_(endpoint), epoll_(epoll) {
//         listenfd_ = ::socket(AF_INET, SOCK_STREAM, 0);
//         if (listenfd_ < 0) throw std::runtime_error("socket failed");

//         int opt = 1;
//         setsockopt(listenfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

//         int flags = fcntl(listenfd_, F_GETFL, 0);
//         fcntl(listenfd_, F_SETFL, flags | O_NONBLOCK);

//         sockaddr_in addr{};
//         addr.sin_family = AF_INET;
//         addr.sin_addr.s_addr = inet_addr(endpoint.ip.data());
//         addr.sin_port = htons(endpoint.port);

//         if (bind(listenfd_, (sockaddr*)&addr, sizeof(addr)) < 0) throw std::runtime_error("bind failed");
//         if (listen(listenfd_, SOMAXCONN) < 0) throw std::runtime_error("listen failed");

//         epoll_->add(listenfd_, EPOLLIN);
//     }

//     Awaitable<Socket> Acceptor::async_accept() {
//         // 等待连接
//         int clientfd = ::accept4(listenfd_, nullptr, nullptr, SOCK_NONBLOCK);
//         if (clientfd < 0) {
//             if (errno == EAGAIN || errno == EWOULDBLOCK) {
//                 co_return Socket{-1, executor_, epoll_};
//             } else {
//                 throw std::runtime_error("accept failed");
//             }
//         }
//         co_return Socket{clientfd, executor_, epoll_};
//     }

//     void Acceptor::close() {
//         // 关闭监听套接字
//         if (listenfd_ >= 0) {
//             epoll_->remove(listenfd_);
//             ::close(listenfd_);
//             listenfd_ = -1;
//         }
//     }

//     Awaitable<size_t> Socket::async_read(Buffer& buffer) {
//         int saveErrno = 0;
//         auto size = buffer.readFd(sockfd_, &saveErrno);
//         if (size < 0) {
//             if (saveErrno == EAGAIN || saveErrno == EWOULDBLOCK) {
//                 epoll_->modify(sockfd_, EPOLLIN);
//                 co_return 0;
//             } else {
//                 throw std::system_error(saveErrno, std::system_category(), "read failed");
//             }
//         }
//         epoll_->modify(sockfd_, EPOLLOUT);
//         co_return size;
//     }

//     Awaitable<size_t> Socket::async_write(Buffer& buffer) {
//         int saveErrno = 0;
//         auto size = buffer.writeFd(sockfd_, &saveErrno);
//         if (size < 0) {
//             if (saveErrno == EAGAIN || saveErrno == EWOULDBLOCK) {
//                 epoll_->modify(sockfd_, EPOLLOUT);
//                 co_return 0;
//             } else {
//                 throw std::system_error(saveErrno, std::system_category(), "write failed");
//             }
//         }
//         epoll_->modify(sockfd_, EPOLLIN);
//         co_return size;
//     }

//     // 关闭套接字
//     void Socket::close() {
//         if (sockfd_ >= 0) {
//             epoll_->remove(sockfd_);
//             ::close(sockfd_);
//             sockfd_ = -1;
//         }
//     }
// }

// #endif // SOCKET_HPP



// // #endif // SOCKET_HPP

#pragma once
#ifndef SOCKET_HPP
#define SOCKET_HPP

#include <system_error>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <io/Buffer.hpp>
#include <Coro/Awaitable.hpp>
#include <net/Epoll.hpp>
#include <tools/IOContext.hpp>

namespace hspd {

struct EndPoint {
    std::string ip;
    uint16_t port;
    EndPoint(std::string ip_ = "0.0.0.0", uint16_t p = 0) : ip(std::move(ip_)), port(p) {}
};

class Socket;

class Acceptor {
public:
    Acceptor(IOContext* ctx, EndPoint ep)
        : ctx_(ctx), endpoint_(std::move(ep)), listenfd_(-1)
    {
        if (!ctx_) throw std::invalid_argument("Acceptor needs IOContext");

        listenfd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (listenfd_ < 0) throw std::system_error(errno, std::system_category(), "socket failed");

        int opt = 1;
        ::setsockopt(listenfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        int flags = ::fcntl(listenfd_, F_GETFL, 0);
        ::fcntl(listenfd_, F_SETFL, flags | O_NONBLOCK);

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(endpoint_.port);
        addr.sin_addr.s_addr = inet_addr(endpoint_.ip.c_str());

        if (bind(listenfd_, (sockaddr*)&addr, sizeof(addr)) < 0)
            throw std::system_error(errno, std::system_category(), "bind failed");

        if (listen(listenfd_, SOMAXCONN) < 0)
            throw std::system_error(errno, std::system_category(), "listen failed");

        // 注册到 epoll（IOContext 封装了 epoll via ev()）
        ctx_->add_fd(listenfd_, EPOLLIN);
    }

    ~Acceptor() { close(); }

    // 协程式 accept（事件驱动）
    inline Awaitable<Socket> async_accept();

    void close() {
        if (listenfd_ >= 0) {
            ctx_->remove_fd(listenfd_);
            ::close(listenfd_);
            listenfd_ = -1;
        }
    }

    EndPoint endpoint() const { return endpoint_; }

    IOContext* context() const { return ctx_; }

private:
    int listenfd_;
    IOContext* ctx_;
    EndPoint endpoint_;
};

class Socket {
public:
    Socket() = default;

    Socket(int fd, IOContext* ctx)
        : sockfd_(fd), ctx_(ctx)
    {
        if (sockfd_ >= 0) {
            // 初识的时候, 注册到epoll, 但是不关注读和写, 只是注册到 epoll
            ctx_->add_fd(sockfd_, 0);
        }
    }

    ~Socket() { close(); }

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket(Socket&& o) noexcept : sockfd_(o.sockfd_), ctx_(o.ctx_) { o.sockfd_ = -1; o.ctx_ = nullptr; }
    Socket& operator=(Socket&& o) noexcept {
        if (this != &o) {
            close();
            sockfd_ = o.sockfd_;
            ctx_ = o.ctx_;
            o.sockfd_ = -1;
            o.ctx_ = nullptr;
        }
        return *this;
    }

    int fd() const noexcept { return sockfd_; }

    // 协程读：立刻尝试 readFd，EAGAIN 则 co_await ctx_->await_fd(fd, EPOLLIN)
    Awaitable<size_t> async_read(Buffer& buffer) {
        while (true) {
            int saveErr = 0;
            auto n = buffer.readFd(sockfd_, &saveErr);
            if (n >= 0) co_return static_cast<size_t>(n);

            if (saveErr == EAGAIN || saveErr == EWOULDBLOCK) {
                co_await ctx_->await_fd(sockfd_, EPOLLIN);
                // 事件就绪后再循环尝试读取
                continue;
            }

            throw std::system_error(saveErr, std::system_category(), "read failed");
        }
    }

    Awaitable<size_t> async_write(Buffer& buffer) {
        while (true) {
            int saveErr = 0;
            auto n = buffer.writeFd(sockfd_, &saveErr);
            if (n >= 0) co_return static_cast<size_t>(n);

            if (saveErr == EAGAIN || saveErr == EWOULDBLOCK) {
                co_await ctx_->await_fd(sockfd_, EPOLLOUT);
                continue;
            }

            throw std::system_error(saveErr, std::system_category(), "write failed");
        }
    }

    void close() {
        if (sockfd_ >= 0) {
            if (ctx_) ctx_->remove_fd(sockfd_);
            ::close(sockfd_);
            sockfd_ = -1;
        }
    }

private:
    int sockfd_ = -1;
    IOContext* ctx_ = nullptr;
};

// ---------------- Acceptor::async_accept 定义 ----------------
inline Awaitable<Socket> Acceptor::async_accept()
{
    while (true) {
        int cfd = ::accept4(listenfd_, nullptr, nullptr, SOCK_NONBLOCK);
        if (cfd >= 0) {
            co_return Socket{ cfd, ctx_ };
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            co_await ctx_->await_fd(listenfd_, EPOLLIN);
            continue;
        }

        throw std::system_error(errno, std::system_category(), "accept failed");
    }
}

} // namespace hspd

#endif // SOCKET_HPP
