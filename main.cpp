#include "include/log/Log.hpp"
#include <log/format.hpp>
#include <string_view>
#include <iostream>
using namespace hspd;
int main() {
    // 创建一个日志格式
    std::string_view f = "The answer is {} + {}";
    std::cout << format(f, 42, 43) << std::endl;

    return 0;
}
