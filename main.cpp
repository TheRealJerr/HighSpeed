#include <iostream>
#include <memory>
#include <string>

#include <tools/ThreadPool.hpp>
#include <net/Epoll.hpp>
#include <tools/IOContext.hpp>
#include <net/Socket.hpp>
#include <Coro/Awaitable.hpp>
#include <io/Buffer.hpp>
#include <log/Log.hpp>

using namespace hspd;

// handle client coroutine
Awaitable<void> handle_client(Socket client) {
    Buffer buf;
    int fd = client.fd();
    size_t n = co_await client.async_read(buf);

    if (n == 0) {
        // EOF or closed: exit coroutine
        LOG_INFO("client fd {} closed", fd);
        client.close();
        co_return;
    }

    // get data as string for demo (Buffer::retrieveAllAsString assumed)
    std::string s = buf.retrieveAllAsString();
    LOG_INFO("received from fd {}: {}", fd, s);

    // echo back
    const std::string html_str = "<html><body><h1>Hello, world!</h1></body></html>";
    buf.append(html_str.c_str(), html_str.size());
    co_await client.async_write(buf);

    client.close();
}

// accept loop coroutine
Awaitable<void> accept_loop(Acceptor* acc) {
    IOContext* ctx = acc->context();
    while (true) {
        Socket client = co_await acc->async_accept();
        LOG_DEBUG("accepted new client fd={}", client.fd());
        // spawn a handler coroutine via IOContext; IOContext::co_spawn will inject ThreadPool* and schedule
        ctx->co_spawn(handle_client(std::move(client)));
    }
}

int main() {

    // create a threadpool (2 worker threads)
    auto pool = ThreadPoolFactory::createThreadPool();

    // create epoll
    auto ep = std::make_unique<Epoll>();

    // create IOContext with pool and epoll
    IOContext io(pool.get(), ep.get());

    // create acceptor on port 8080
    Acceptor acc(&io, EndPoint{"0.0.0.0", 8080});

    // start threadpool workers
    pool->run();

    // start accept coroutine (scheduled on threadpool via IOContext::co_spawn)
    io.co_spawn(accept_loop(&acc));

    // run epoll loop in main thread
    io.run();

    return 0;
}
