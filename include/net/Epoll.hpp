#ifndef EPOLL_HPP
#define EPOLL_HPP

#include <tools/ThreadPool.hpp>
#include <coro/Awaitable.hpp>
#include <io/Buffer.hpp>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <string_view>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <iostream>
#include <net/Epoll.hpp>

class Epoll {
public:
    Epoll() {
        epoll_fd_ = epoll_create1(0);
        if (epoll_fd_ < 0) {
            throw std::runtime_error("epoll_create1 failed");
        }
    }

    ~Epoll() {
        close(epoll_fd_);
    }

    void add(int fd, uint32_t events) {
        struct epoll_event ev;
        ev.events = events;
        ev.data.fd = fd;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0) {
            throw std::runtime_error("epoll_ctl add failed");
        }
    }

    void modify(int fd, uint32_t events) {
        struct epoll_event ev;
        ev.events = events;
        ev.data.fd = fd;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev) < 0) {
            throw std::runtime_error("epoll_ctl modify failed");
        }
    }

    void remove(int fd) {
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) < 0) {
            throw std::runtime_error("epoll_ctl remove failed");
        }
    }

    int wait(struct epoll_event* events, int max_events, int timeout) {
        return epoll_wait(epoll_fd_, events, max_events, timeout);
    }

private:
    int epoll_fd_;
};

#endif // EPOLL_HPP