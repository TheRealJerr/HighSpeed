
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
#include <net/IOContext.hpp>

namespace hspd {

struct EndPoint {
    std::string_view ip;
    uint16_t port;
    EndPoint(std::string_view ip_ = "0.0.0.0", uint16_t p = 0) : ip(ip_), port(p) {}
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
        addr.sin_addr.s_addr = inet_addr(endpoint_.ip.data());

        if (bind(listenfd_, (sockaddr*)&addr, sizeof(addr)) < 0)
            throw std::system_error(errno, std::system_category(), "bind failed");

        if (listen(listenfd_, SOMAXCONN) < 0)
            throw std::system_error(errno, std::system_category(), "listen failed");

        // 将监听套接字注册到 epoll 模型中
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
            // 连接到来了, 我只关注读
            ctx_->add_fd(sockfd_, EPOLLIN);
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
            // 当dispatch调度我的时候, 我直接开始读, 直接将内容全部读完
            int saveErr = 0;
            auto n = buffer.readFd(sockfd_, &saveErr);
            if (n >= 0) 
            {
                // 这里的问题就是, 可能没有没有注册对应的调度协程, 可能出现空转
                // 只要读到了内容
                // 我现在只关注写
                // ctx_->modify_fd(sockfd_, EPOLLOUT);
                LOG_INFO("{} async_read : {} bytes", sockfd_, n);
                co_return static_cast<size_t>(n);
            }

            if (saveErr == EAGAIN || saveErr == EWOULDBLOCK) {
                // 如果读取没有立马就绪, 我们就将对应的读取写成进行注入
                LOG_INFO("{} async_read EAGAIN : ", sockfd_);
                // 在原来的基础上添加EPOLLIN
                uint32_t modify_events = ctx_->get_events(sockfd_) | EPOLLIN;
                co_await ctx_->await_fd(sockfd_, modify_events);                // 事件就绪后再循环尝试读取
                continue;
            }

            throw std::system_error(saveErr, std::system_category(), "read failed");
        }
    }

    Awaitable<size_t> async_write(Buffer& buffer) {
        while (true) {
            // 做一个特殊的判断
            if(buffer.readableBytes() == 0)
                co_return 0;
            int saveErr = 0;
            auto n = buffer.writeFd(sockfd_, &saveErr);
            if (n >= 0) 
            {
                LOG_INFO("{} async_write : {} bytes", sockfd_, n);
                co_return static_cast<size_t>(n);
            }

            if (saveErr == EAGAIN || saveErr == EWOULDBLOCK) {
                auto prev_events = ctx_->get_events(sockfd_);
                // 在原来的基础上添加EPOLLOUT
                uint32_t modify_events = prev_events | EPOLLOUT;
                co_await ctx_->await_fd(sockfd_, modify_events);
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
        // 监听成功
        if (cfd >= 0) {
            co_return Socket{ cfd, ctx_ };
        }
        // 非阻塞监听失败, 
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // 挂起协程, 等待epoll模型的下一次调用
            co_await ctx_->await_fd(listenfd_, EPOLLIN);
            continue;
        }
        // handle Error
        throw std::system_error(errno, std::system_category(), "accept failed");
    }
}

} // namespace hspd

#endif // SOCKET_HPP
