#ifndef FORMAT_HPP
#define FORMAT_HPP

#include <sstream>
#include <string>
#include <tuple>

namespace hspd
{
    template<typename... Args>
    std::string format(const std::string& fmt, Args... args) {
        std::stringstream ss;
        ss << fmt;
        auto format_helper = [&](const auto& arg) {
            size_t pos = ss.str().find("{}");
            if (pos != std::string::npos) {
                ss.seekp(pos);
                ss << arg;
                ss.seekp(pos + 2);
            }
        };
        (format_helper(args), ...);
        return ss.str();
    }
    template <typename... Args>
    std::string format(std::string_view fmt,Args&&... args)
    {
        return format(std::string(fmt), std::forward<Args>(args)...);
    }

}
#endif // FORMAT_HPP
