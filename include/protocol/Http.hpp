#ifndef HTTP_HPP
#define HTTP_HPP
#include <string>
#include <string_view>
#include <unordered_map>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>

namespace hspd {

class Message {
public:
    virtual ~Message() = default;
    virtual std::string serialize_to_string() const = 0;
    virtual bool parse_from_string(const std::string& str) = 0;
    virtual bool parse_from_string(const char* str, size_t len) = 0;
};

enum class HttpMethod {
    GET, POST, PUT, DELETE, HEAD, OPTIONS, TRACE, CONNECT, PATCH,
    UNKNOWN,
};

enum class HttpVersion {
    HTTP_1_0,
    HTTP_1_1,
    HTTP_2_0,
    UNKNOWN
};

class HttpMessage : public Message {
public:
    virtual ~HttpMessage() = default;

    std::string serialize_to_string() const override;
    bool parse_from_string(const std::string& str) override;
    bool parse_from_string(const char* str, size_t len) override;

    const HttpMethod& method() const { return method_; }
    const HttpVersion& version() const { return version_; }
    const std::string& url() const { return url_; }
    const std::unordered_map<std::string, std::string>& headers() const { return headers_; }
    const std::string& body() const { return body_; }

    void set_method(const HttpMethod& method) { method_ = method; }
    void set_version(const HttpVersion& version) { version_ = version; }
    void set_url(const std::string& url) { url_ = url; }
    void set_headers(const std::unordered_map<std::string, std::string>& headers) { headers_ = headers; }
    void set_body(const std::string& body) { body_ = body; }
    void add_header(const std::string& key, const std::string& value) { headers_[key] = value; }

    static HttpMethod parse_method(const std::string& s);
    static HttpVersion parse_version(const std::string& s);
    static std::string version_to_str(HttpVersion v);
    static std::string method_to_str(HttpMethod m);
private:
    HttpMethod method_;
    HttpVersion version_;
    std::string url_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;

    

    static inline constexpr std::string_view line_sep = "\r\n";

};


HttpMethod HttpMessage::parse_method(const std::string& s)
{
    if (s == "GET") return HttpMethod::GET;
    if (s == "POST") return HttpMethod::POST;
    if (s == "PUT") return HttpMethod::PUT;
    if (s == "DELETE") return HttpMethod::DELETE;
    if (s == "HEAD") return HttpMethod::HEAD;
    if (s == "OPTIONS") return HttpMethod::OPTIONS;
    if (s == "TRACE") return HttpMethod::TRACE;
    if (s == "CONNECT") return HttpMethod::CONNECT;
    if (s == "PATCH") return HttpMethod::PATCH;

    return HttpMethod::UNKNOWN;
}

HttpVersion HttpMessage::parse_version(const std::string& s)
{
    if (s == "HTTP/1.0") return HttpVersion::HTTP_1_0;
    if (s == "HTTP/1.1") return HttpVersion::HTTP_1_1;
    if (s == "HTTP/2.0") return HttpVersion::HTTP_2_0;
    return HttpVersion::UNKNOWN;
}

std::string HttpMessage::version_to_str(HttpVersion v)
{
    switch (v) {
        case HttpVersion::HTTP_1_0: return "HTTP/1.0";
        case HttpVersion::HTTP_1_1: return "HTTP/1.1";
        case HttpVersion::HTTP_2_0: return "HTTP/2.0";
        default: return "HTTP/1.1";
    }
}

std::string HttpMessage::method_to_str(HttpMethod m)
{
    switch (m) {
        case HttpMethod::GET: return "GET";
        case HttpMethod::POST: return "POST";
        case HttpMethod::PUT: return "PUT";
        case HttpMethod::DELETE: return "DELETE";
        case HttpMethod::HEAD: return "HEAD";
        case HttpMethod::OPTIONS: return "OPTIONS";
        case HttpMethod::TRACE: return "TRACE";
        case HttpMethod::CONNECT: return "CONNECT";
        case HttpMethod::PATCH: return "PATCH";
        default: return "UNKNOWN";
    }
}


//=================== serialize ===================//

std::string HttpMessage::serialize_to_string() const
{
    std::ostringstream ss;

    // 起始行：GET /path HTTP/1.1
    ss << method_to_str(method_) << " " << url_ << " " << version_to_str(version_) << line_sep;

    // header
    for (const auto& [k, v] : headers_) {
        ss << k << ": " << v << line_sep;
    }

    ss << line_sep;

    // body
    ss << body_;

    return ss.str();
}


//=================== parse ===================//

bool HttpMessage::parse_from_string(const std::string& str)
{
    return parse_from_string(str.c_str(), str.size());
}

bool HttpMessage::parse_from_string(const char* str, size_t len)
{
    std::string data(str, len);

    size_t pos = data.find(line_sep);
    if (pos == std::string::npos)
        return false;

    //========== 1. 解析起始行 ==========
    std::string start_line = data.substr(0, pos);

    {
        std::istringstream ss(start_line);
        std::string method_s, url_s, version_s;

        ss >> method_s >> url_s >> version_s;

        method_ = parse_method(method_s);
        url_ = url_s;
        version_ = parse_version(version_s);
    }

    //========== 2. 解析 headers ==========
    size_t header_start = pos + line_sep.size();
    size_t header_end = data.find("\r\n\r\n");

    if (header_end == std::string::npos)
        return false;

    headers_.clear();

    std::string header_block = data.substr(header_start, header_end - header_start);

    std::istringstream hs(header_block);
    std::string line;
    while (std::getline(hs, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        size_t colon = line.find(":");
        if (colon == std::string::npos) continue;

        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        value.erase(0, value.find_first_not_of(" "));

        headers_[key] = value;
    }

    //========== 3. 解析 body ==========
    body_ = data.substr(header_end + line_sep.size() * 2);

    return true;
}

}

#endif // HTTP_HPP