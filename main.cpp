#include <iostream>
#include "Generator.hpp"

// 生成斐波那契数列
Generator<int> fibonacci(int n)
{
    int a = 0, b = 1;
    for (int i = 0; i < n; ++i) {
        co_yield a;
        auto next = a + b;
        a = b;
        b = next;
    }
}

// 生成范围序列
Generator<int> range(int start, int end, int step = 1)
{
    for (int i = start; i < end; i += step) {
        co_yield i;
    }
}

// 测试拷贝的类
class testCopyObject
{
public:
    testCopyObject(int data) : data_(data) {}

    testCopyObject(const testCopyObject& other) : data_(other.data_) {
        std::cout << "Copy constructor called." << std::endl;
    }

    testCopyObject& operator=(const testCopyObject& other) {
        data_ = other.data_;
        std::cout << "Copy assignment operator called." << std::endl;
        return *this;
    }

    int data() const {
        return data_;
    }
    testCopyObject(testCopyObject&& other) noexcept : data_(other.data_) {
        std::cout << "Move constructor called." << std::endl;
    }

    testCopyObject& operator=(testCopyObject&& other) noexcept {
        data_ = other.data_;
        std::cout << "Move assignment operator called." << std::endl;
        return *this;
    }
private:
    int data_;

};

Generator<testCopyObject> testCopy()
{
    for(int i = 0;i < 10;i++)
        co_yield testCopyObject(i);
    co_return;
}

int main()
{
    for(auto& obj : testCopy())
    {
        std::cout << obj.data() << std::endl;
    }

    return 0;
}