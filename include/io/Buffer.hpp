#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <unistd.h>
#include <sys/uio.h>

class Buffer {
public:
    explicit Buffer(size_t initialSize = 1024);

    size_t readableBytes() const { return writeIndex_ - readIndex_; }
    size_t writableBytes() const { return buffer_.size() - writeIndex_; }
    size_t prependableBytes() const { return readIndex_; }

    /// 可读数据的起始位置
    const char* peek() const { return buffer_.data() + readIndex_; }

    /// 取走 len 字节
    void retrieve(size_t len);
    void retrieveAll();
    std::string retrieveAsString(size_t len);
    std::string retrieveAllAsString();

    /// 添加数据到 Buffer
    void append(const char* data, size_t len);
    void append(const std::string& str) { append(str.data(), str.size()); }

    /// 写 socket
    ssize_t writeFd(int fd, int* savedErrno);

    /// 从 socket 读
    ssize_t readFd(int fd, int* savedErrno);

    /// 返回可写起点
    char* beginWrite() { return buffer_.data() + writeIndex_; }
    const char* beginWrite() const { return buffer_.data() + writeIndex_; }

private:
    void makeSpace(size_t len);

private:
    std::vector<char> buffer_;
    size_t readIndex_;
    size_t writeIndex_;
    static const size_t kCheapPrepend = 8;
};


Buffer::Buffer(size_t initialSize)
    : buffer_(kCheapPrepend + initialSize),
      readIndex_(kCheapPrepend),
      writeIndex_(kCheapPrepend) {}

void Buffer::retrieve(size_t len) {
    if (len < readableBytes()) {
        readIndex_ += len;
    } else {
        retrieveAll();
    }
}

void Buffer::retrieveAll() {
    readIndex_ = writeIndex_ = kCheapPrepend;
}

std::string Buffer::retrieveAsString(size_t len) {
    len = std::min(len, readableBytes());
    std::string result(peek(), len);
    retrieve(len);
    return result;
}

std::string Buffer::retrieveAllAsString() {
    return retrieveAsString(readableBytes());
}

void Buffer::append(const char* data, size_t len) {
    if (len > writableBytes()) {
        makeSpace(len);
    }
    std::copy(data, data + len, beginWrite());
    writeIndex_ += len;
}

void Buffer::makeSpace(size_t len) {
    if (writableBytes() + prependableBytes() < len + kCheapPrepend) {
        // 扩容
        buffer_.resize(writeIndex_ + len);
    } else {
        // 数据前移
        size_t readable = readableBytes();
        std::copy(buffer_.data() + readIndex_,
                  buffer_.data() + writeIndex_,
                  buffer_.data() + kCheapPrepend);
        readIndex_ = kCheapPrepend;
        writeIndex_ = readIndex_ + readable;
    }
}

ssize_t Buffer::readFd(int fd, int* savedErrno) {
    // 使用 readv：优先读入剩余空间，不够则读入栈上的临时缓冲
    char extraBuf[65536];
    struct iovec vecs[2];

    const size_t writable = writableBytes();

    vecs[0].iov_base = beginWrite();
    vecs[0].iov_len  = writable;

    vecs[1].iov_base = extraBuf;
    vecs[1].iov_len  = sizeof(extraBuf);

    const int iovcnt = writable < sizeof(extraBuf) ? 2 : 1;
    ssize_t n = ::readv(fd, vecs, iovcnt);

    if (n < 0) {
        *savedErrno = errno;
    } else if (n <= (ssize_t)writable) {
        writeIndex_ += n;
    } else {
        writeIndex_ = buffer_.size();
        append(extraBuf, n - writable);
    }

    return n;
}

ssize_t Buffer::writeFd(int fd, int* savedErrno) {
    ssize_t n = ::write(fd, peek(), readableBytes());
    if (n < 0) {
        *savedErrno = errno;
        return n;
    }
    retrieve(n);
    return n;
}