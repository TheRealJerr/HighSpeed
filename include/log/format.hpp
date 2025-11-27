#ifndef FORMAT_H
#define FORMAT_H

#include <string>
#include <string_view>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace hspd {

// 格式化异常类
class format_error : public std::runtime_error {
public:
    explicit format_error(const std::string& msg) : std::runtime_error(msg) {}
};

namespace detail {

// 类型特征检查
template<typename T>
struct is_string_like : std::false_type {};

template<>
struct is_string_like<std::string> : std::true_type {};

template<>
struct is_string_like<std::string_view> : std::true_type {};

template<>
struct is_string_like<const char*> : std::true_type {};

template<size_t N>
struct is_string_like<char[N]> : std::true_type {};

template<typename T>
inline constexpr bool is_string_like_v = is_string_like<T>::value;

// 格式化参数基类
class format_arg_base {
public:
    virtual ~format_arg_base() = default;
    virtual void format(std::ostream& os, std::string_view spec) const = 0;
};

// 格式化参数包装器
template<typename T>
class format_arg : public format_arg_base {
public:
    explicit format_arg(const T& value) : value_(value) {}
    
    void format(std::ostream& os, std::string_view spec) const override {
        format_impl(os, spec, value_);
    }

private:
    const T& value_;
    
    // 整数类型格式化
    template<typename U>
    static std::enable_if_t<std::is_integral_v<U>> 
    format_impl(std::ostream& os, std::string_view spec, const U& value) {
        if (spec.empty()) {
            os << value;
            return;
        }
        
        // 简单的格式说明符处理
        switch (spec[0]) {
            case 'x': case 'X':
                os << std::hex;
                if (spec[0] == 'X') os << std::uppercase;
                os << value;
                os << std::dec << std::nouppercase;
                break;
            case 'o':
                os << std::oct << value << std::dec;
                break;
            case 'b':
                // 二进制输出（简单实现）
                os << "0b";
                for (int i = sizeof(U) * 8 - 1; i >= 0; --i) {
                    os << ((value >> i) & 1);
                }
                break;
            default:
                os << value;
        }
    }
    
    // 浮点数类型格式化
    template<typename U>
    static std::enable_if_t<std::is_floating_point_v<U>> 
    format_impl(std::ostream& os, std::string_view spec, const U& value) {
        if (spec.empty()) {
            os << value;
            return;
        }
        
        // 简单的浮点数格式说明符
        if (spec == "f" || spec == "F") {
            os << std::fixed << value;
        } else if (spec == "e" || spec == "E") {
            os << std::scientific;
            if (spec == "E") os << std::uppercase;
            os << value;
            os << std::nouppercase;
        } else if (spec == "g" || spec == "G") {
            os << std::defaultfloat;
            if (spec == "G") os << std::uppercase;
            os << value;
            os << std::nouppercase;
        } else {
            // 尝试解析精度
            try {
                if (spec[0] == '.') {
                    int precision = std::stoi(std::string(spec.substr(1)));
                    os << std::fixed << std::setprecision(precision) << value;
                } else {
                    os << value;
                }
            } catch (...) {
                os << value;
            }
        }
    }
    
    // 字符串类型格式化
    template<typename U>
    static std::enable_if_t<is_string_like_v<U>> 
    format_impl(std::ostream& os, std::string_view spec, const U& value) {
        if (spec.empty()) {
            os << value;
            return;
        }
        
        // 简单的字符串格式说明符
        if (spec == "s") {
            os << value;
        } else {
            // 未知格式说明符，直接输出
            os << value;
        }
    }
    
    // 指针类型格式化
    static void format_impl(std::ostream& os, std::string_view spec, const void* value) {
        if (spec.empty() || spec == "p") {
            os << value;
        } else {
            os << value;
        }
    }
    
    // 布尔类型格式化
    static void format_impl(std::ostream& os, std::string_view spec, bool value) {
        if (spec.empty()) {
            os << std::boolalpha << value;
        } else if (spec == "d") {
            os << (value ? 1 : 0);
        } else if (spec == "s") {
            os << (value ? "true" : "false");
        } else {
            os << std::boolalpha << value;
        }
    }
};

// 格式化上下文
class format_context {
public:
    template<typename... Args>
    format_context(const Args&... args) {
        reserve(sizeof...(Args));
        (emplace_back(args), ...);
    }
    
    ~format_context() {
        for (auto* arg : args_) {
            delete arg;
        }
    }
    
    const format_arg_base* get_arg(size_t index) const {
        if (index >= args_.size()) {
            throw format_error("Argument index out of range");
        }
        return args_[index];
    }
    
    size_t size() const { return args_.size(); }

private:
    std::vector<format_arg_base*> args_;
    
    void reserve(size_t size) {
        args_.reserve(size);
    }
    
    template<typename T>
    void emplace_back(const T& value) {
        args_.push_back(new format_arg<T>(value));
    }
};

// 解析格式字符串并生成结果
std::string vformat(std::string_view fmt, const format_context& ctx) {
    std::ostringstream oss;
    size_t pos = 0;
    size_t len = fmt.length();
    size_t arg_index = 0;
    
    while (pos < len) {
        // 查找下一个 '{' 或 '}'
        size_t next_brace = fmt.find_first_of("{}", pos);
        
        if (next_brace == std::string_view::npos) {
            // 没有更多的花括号，添加剩余文本
            oss << fmt.substr(pos);
            break;
        }
        
        // 添加花括号前的文本
        oss << fmt.substr(pos, next_brace - pos);
        
        char brace = fmt[next_brace];
        
        if (brace == '}') {
            // 检查是否是转义的 '}'
            if (next_brace + 1 < len && fmt[next_brace + 1] == '}') {
                oss << '}';
                pos = next_brace + 2;
            } else {
                throw format_error("Unmatched '}' in format string");
            }
        } else { // brace == '{'
            // 检查是否是转义的 '{'
            if (next_brace + 1 < len && fmt[next_brace + 1] == '{') {
                oss << '{';
                pos = next_brace + 2;
                continue;
            }
            
            // 解析格式说明符
            size_t format_start = next_brace + 1;
            size_t format_end = fmt.find('}', format_start);
            
            if (format_end == std::string_view::npos) {
                throw format_error("Unmatched '{' in format string");
            }
            
            std::string_view format_spec = fmt.substr(format_start, format_end - format_start);
            
            if (format_spec.empty()) {
                // 自动编号: {}
                if (arg_index >= ctx.size()) {
                    throw format_error("Too few arguments provided");
                }
                ctx.get_arg(arg_index)->format(oss, "");
                arg_index++;
            } else {
                // 手动编号: {0} 或带格式说明符: {0:x}
                size_t colon_pos = format_spec.find(':');
                std::string_view index_str = format_spec.substr(0, colon_pos);
                std::string_view spec_str = (colon_pos == std::string_view::npos) ? 
                                           std::string_view() : 
                                           format_spec.substr(colon_pos + 1);
                
                // 解析参数索引
                size_t index;
                try {
                    index = std::stoul(std::string(index_str));
                } catch (...) {
                    throw format_error("Invalid argument index: " + std::string(index_str));
                }
                
                if (index >= ctx.size()) {
                    throw format_error("Argument index out of range: " + std::to_string(index));
                }
                
                ctx.get_arg(index)->format(oss, spec_str);
            }
            
            pos = format_end + 1;
        }
    }
    
    // 检查是否所有参数都被使用
    if (arg_index < ctx.size()) {
        throw format_error("Too many arguments provided");
    }
    
    return oss.str();
}

} // namespace detail

// 主格式化函数
template<typename... Args>
std::string format(std::string_view fmt, const Args&... args) {
    detail::format_context ctx(args...);
    return detail::vformat(fmt, ctx);
}

} // namespace my

#endif // FORMAT_H