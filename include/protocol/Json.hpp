#ifndef JSON_HPP
#define JSON_HPP

#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <cctype>
#include <sstream>
#include <utility>
#include <iostream>

namespace hspd {

enum class JsonType {
    Object,
    Array,
    String,
    Number,
    Boolean,
    Null
};

class JsonObject;
class JsonArray;

// ------------------------- JsonBase -------------------------
class JsonBase {
public:
    virtual JsonType getType() const = 0;
    virtual std::string toString() const = 0;
    virtual std::string dump(int indent = -1, int depth = 0) const = 0;
    virtual ~JsonBase() = default;

    // type converters (override in subclasses as needed)
    virtual JsonObject* asObject() { throw std::runtime_error("Not an object"); }
    virtual JsonArray* asArray() { throw std::runtime_error("Not an array"); }
    virtual std::string asString() { throw std::runtime_error("Not a string"); }
    virtual double asNumber() { throw std::runtime_error("Not a number"); }
    virtual bool asBool() { throw std::runtime_error("Not a boolean"); }
};

// ------------------------- JsonString -------------------------
class JsonString : public JsonBase {
public:
    explicit JsonString(std::string v) : value_(std::move(v)) {}
    JsonType getType() const override { return JsonType::String; }
    std::string toString() const override {
        std::string out = "\"";
        for (char c : value_) {
            switch (c) {
                case '\"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default: out += c; break;
            }
        }
        out += "\"";
        return out;
    }
    std::string dump(int indent, int depth) const override { return toString(); }
    std::string asString() override { return value_; }
    const std::string& getValue() const { return value_; }
private:
    std::string value_;
};

// ------------------------- JsonNumber -------------------------
class JsonNumber : public JsonBase {
public:
    explicit JsonNumber(double v) : value_(v) {}
    JsonType getType() const override { return JsonType::Number; }
    std::string toString() const override {
        std::ostringstream oss;
        oss << value_;
        return oss.str();
    }
    std::string dump(int indent, int depth) const override { return toString(); }
    double asNumber() override { return value_; }
    double getValue() const { return value_; }
private:
    double value_;
};

// ------------------------- JsonBoolean -------------------------
class JsonBoolean : public JsonBase {
public:
    explicit JsonBoolean(bool v) : value_(v) {}
    JsonType getType() const override { return JsonType::Boolean; }
    std::string toString() const override { return value_ ? "true" : "false"; }
    std::string dump(int indent, int depth) const override { return toString(); }
    bool asBool() override { return value_; }
    bool getValue() const { return value_; }
private:
    bool value_;
};

// ------------------------- JsonNull -------------------------
class JsonNull : public JsonBase {
public:
    JsonType getType() const override { return JsonType::Null; }
    std::string toString() const override { return "null"; }
    std::string dump(int indent, int depth) const override { return "null"; }
};

// ------------------------- JsonObject -------------------------
class JsonObject : public JsonBase {
public:
    using Member = std::pair<std::string, std::unique_ptr<JsonBase>>;
    using Members = std::vector<Member>;

    JsonObject() = default;
    explicit JsonObject(Members m) : members_(std::move(m)) {}

    JsonType getType() const override { return JsonType::Object; }
    JsonObject* asObject() override { return this; }

    const Members& getMembers() const { return members_; }

    bool contains(const std::string& key) const {
        for (const auto& m : members_) if (m.first == key) return true;
        return false;
    }

    // return reference to underlying unique_ptr for JsonValue to hold
    std::unique_ptr<JsonBase>& getRefForKeyCreate(const std::string& key) {
        for (auto& m : members_) {
            if (m.first == key) return m.second;
        }
        members_.emplace_back(key, std::make_unique<JsonNull>());
        return members_.back().second;
    }

    // const access
    const JsonBase& operator[](const std::string& key) const {
        for (const auto& m : members_) if (m.first == key) return *m.second;
        throw std::runtime_error("Key not found: " + key);
    }

    // insert (alias add)
    void insert(std::string key, std::unique_ptr<JsonBase> val) {
        members_.emplace_back(std::move(key), std::move(val));
    }
    void add(std::string key, std::unique_ptr<JsonBase> val) { insert(std::move(key), std::move(val)); }

    std::string toString() const override {
        std::string result = "{";
        bool first = true;
        for (const auto& m : members_) {
            if (!first) result += ",";
            result += "\"" + m.first + "\":" + m.second->toString();
            first = false;
        }
        result += "}";
        return result;
    }

    std::string dump(int indent, int depth) const override {
        if (indent < 0) return toString();
        std::string pad(depth * indent, ' ');
        std::string inside((depth + 1) * indent, ' ');
        std::string res = "{\n";
        for (size_t i = 0; i < members_.size(); ++i) {
            res += inside + "\"" + members_[i].first + "\": " + members_[i].second->dump(indent, depth + 1);
            if (i + 1 < members_.size()) res += ",";
            res += "\n";
        }
        res += pad + "}";
        return res;
    }

private:
    Members members_;
    friend class JsonValue;
};

// ------------------------- JsonArray -------------------------
class JsonArray : public JsonBase {
public:
    using Elements = std::vector<std::unique_ptr<JsonBase>>;
    JsonArray() = default;
    explicit JsonArray(Elements e) : elements_(std::move(e)) {}

    JsonType getType() const override { return JsonType::Array; }
    JsonArray* asArray() override { return this; }

    const Elements& getElements() const { return elements_; }

    // return reference to element's unique_ptr (for JsonValue)
    std::unique_ptr<JsonBase>& getElementRefCreate(size_t idx) {
        if (idx >= elements_.size()) {
            // expand and fill with nulls
            elements_.resize(idx + 1);
            for (size_t i = 0; i < elements_.size(); ++i)
                if (!elements_[i]) elements_[i] = std::make_unique<JsonNull>();
        }
        if (!elements_[idx]) elements_[idx] = std::make_unique<JsonNull>();
        return elements_[idx];
    }

    const JsonBase& operator[](size_t idx) const {
        if (idx >= elements_.size()) throw std::runtime_error("Array index out of range");
        return *elements_[idx];
    }

    std::string toString() const override {
        std::string result = "[";
        bool first = true;
        for (const auto& e : elements_) {
            if (!first) result += ",";
            result += e ? e->toString() : "null";
            first = false;
        }
        result += "]";
        return result;
    }

    std::string dump(int indent, int depth) const override {
        if (indent < 0) return toString();
        std::string pad(depth * indent, ' ');
        std::string inside((depth + 1) * indent, ' ');
        std::string res = "[\n";
        for (size_t i = 0; i < elements_.size(); ++i) {
            res += inside + (elements_[i] ? elements_[i]->dump(indent, depth + 1) : "null");
            if (i + 1 < elements_.size()) res += ",";
            res += "\n";
        }
        res += pad + "]";
        return res;
    }

    void push(std::unique_ptr<JsonBase> v) { elements_.push_back(std::move(v)); }

    size_t size() const { return elements_.size(); }

private:
    Elements elements_;
    friend class JsonValue;
};

// ------------------------- JsonValue (代理) -------------------------
class JsonValue {
public:
    // construct from pointer to owning unique_ptr<JsonBase>
    explicit JsonValue(std::unique_ptr<JsonBase>* ref_ptr) : ref_ptr_(ref_ptr) {
        if (!ref_ptr_) throw std::runtime_error("Null reference in JsonValue");
        if (!*ref_ptr_) {
            // initialize with null if unset
            *ref_ptr_ = std::make_unique<JsonNull>();
        }
    }
    // 自己私有一个对象直接构造
    explicit JsonValue() : ref_ptr_(new std::unique_ptr<JsonBase>(new JsonObject())) {}

    // Copyable (pointer semantics)
    JsonValue(const JsonValue& other) = default;
    JsonValue& operator=(const JsonValue& other) = default;

    // Assignments: various types
    JsonValue& operator=(const char* s) { return assignString(std::string(s)); }
    JsonValue& operator=(const std::string& s) { return assignString(s); }
    JsonValue& operator=(double d) { return assignNumber(d); }
    JsonValue& operator=(float f) { return assignNumber(static_cast<double>(f)); }
    JsonValue& operator=(int i) { return assignNumber(static_cast<double>(i)); }
    JsonValue& operator=(long l) { return assignNumber(static_cast<double>(l)); }
    JsonValue& operator=(bool b) { return assignBool(b); }
    JsonValue& operator=(std::nullptr_t) { return assignNull(); }

    // assign from JsonObject (by move)
    JsonValue& operator=(JsonObject&& obj) {
        *ref_ptr_ = std::make_unique<JsonObject>(std::move(obj));
        return *this;
    }
    // assign from JsonArray (by move)
    JsonValue& operator=(JsonArray&& arr) {
        *ref_ptr_ = std::make_unique<JsonArray>(std::move(arr));
        return *this;
    }
    // assign from unique_ptr<JsonBase>
    JsonValue& operator=(std::unique_ptr<JsonBase> up) {
        *ref_ptr_ = std::move(up);
        if (!*ref_ptr_) *ref_ptr_ = std::make_unique<JsonNull>();
        return *this;
    }

    // get underlying JsonBase pointer (may be null)
    JsonBase* get() const { return ref_ptr_ && *ref_ptr_ ? ref_ptr_->get() : nullptr; }

    // Type accessors with safety
    JsonObject& asObject() {
        ensureExistsAs(JsonType::Object);
        return *static_cast<JsonObject*>(ref_ptr_->get());
    }
    JsonArray& asArray() {
        ensureExistsAs(JsonType::Array);
        return *static_cast<JsonArray*>(ref_ptr_->get());
    }
    std::string asString() {
        if (!get()) throw std::runtime_error("Value is null");
        return get()->asString();
    }
    double asNumber() {
        if (!get()) throw std::runtime_error("Value is null");
        return get()->asNumber();
    }
    bool asBool() {
        if (!get()) throw std::runtime_error("Value is null");
        return get()->asBool();
    }

    // read-only type test
    JsonType type() const {
        if (!get()) return JsonType::Null;
        return get()->getType();
    }

    // operator[] for nested access (string)
    JsonValue operator[](const std::string& key) {
        // if not object, replace with object
        if (!get() || get()->getType() != JsonType::Object) {
            *ref_ptr_ = std::make_unique<JsonObject>();
        }
        JsonObject* obj = static_cast<JsonObject*>(ref_ptr_->get());
        auto& child_ref = obj->getRefForKeyCreate(key);
        return JsonValue(&child_ref);
    }

    // operator[] for array index
    JsonValue operator[](size_t idx) {
        // if not array, replace with array
        if (!get() || get()->getType() != JsonType::Array) {
            *ref_ptr_ = std::make_unique<JsonArray>();
        }
        JsonArray* arr = static_cast<JsonArray*>(ref_ptr_->get());
        auto& child_ref = arr->getElementRefCreate(idx);
        return JsonValue(&child_ref);
    }

    // push value to array (convenience)
    void push(const char* s) { ensureArray(); asArray().push(std::make_unique<JsonString>(std::string(s))); }
    void push(const std::string& s) { ensureArray(); asArray().push(std::make_unique<JsonString>(s)); }
    void push(double d) { ensureArray(); asArray().push(std::make_unique<JsonNumber>(d)); }
    void push(int i) { ensureArray(); asArray().push(std::make_unique<JsonNumber>(static_cast<double>(i))); }
    void push(bool b) { ensureArray(); asArray().push(std::make_unique<JsonBoolean>(b)); }
    void push(std::unique_ptr<JsonBase> v) { ensureArray(); asArray().push(std::move(v)); }

    // stringify / dump
    std::string toString() const {
        if (!get()) return "null";
        return get()->toString();
    }
    std::string dump(int indent = -1, int depth = 0) const {
        if (!get()) return "null";
        return get()->dump(indent, depth);
    }

private:
    std::unique_ptr<JsonBase>* ref_ptr_{nullptr};

    // internal helpers
    JsonValue& assignString(const std::string& s) {
        *ref_ptr_ = std::make_unique<JsonString>(s);
        return *this;
    }
    JsonValue& assignNumber(double d) {
        *ref_ptr_ = std::make_unique<JsonNumber>(d);
        return *this;
    }
    JsonValue& assignBool(bool b) {
        *ref_ptr_ = std::make_unique<JsonBoolean>(b);
        return *this;
    }
    JsonValue& assignNull() {
        *ref_ptr_ = std::make_unique<JsonNull>();
        return *this;
    }

    void ensureExistsAs(JsonType t) {
        if (!get() || get()->getType() != t) {
            if (t == JsonType::Object) *ref_ptr_ = std::make_unique<JsonObject>();
            else if (t == JsonType::Array) *ref_ptr_ = std::make_unique<JsonArray>();
            else if (t == JsonType::String) *ref_ptr_ = std::make_unique<JsonString>(std::string());
            else if (t == JsonType::Number) *ref_ptr_ = std::make_unique<JsonNumber>(0.0);
            else if (t == JsonType::Boolean) *ref_ptr_ = std::make_unique<JsonBoolean>(false);
            else *ref_ptr_ = std::make_unique<JsonNull>();
        }
    }
    void ensureArray() {
        if (!get() || get()->getType() != JsonType::Array) *ref_ptr_ = std::make_unique<JsonArray>();
    }
};

// ------------------------- JSON Parser -------------------------
class JsonParser {
public:
    static std::unique_ptr<JsonBase> parse(const std::string& json) {
        size_t pos = 0;
        skipSpaces(json, pos);
        auto root = parseValue(json, pos);
        skipSpaces(json, pos);
        // accept trailing spaces only
        if (pos != json.size()) throw std::runtime_error("Extra characters after JSON end");
        return root;
    }

private:
    static void skipSpaces(const std::string& s, size_t& pos) {
        while (pos < s.size() && static_cast<unsigned char>(s[pos]) && isspace(static_cast<unsigned char>(s[pos]))) ++pos;
    }

    static std::unique_ptr<JsonBase> parseValue(const std::string& json, size_t& pos) {
        skipSpaces(json, pos);
        if (pos >= json.size()) throw std::runtime_error("Unexpected end of JSON");
        char c = json[pos];
        if (c == '{') return parseObject(json, pos);
        if (c == '[') return parseArray(json, pos);
        if (c == '"') return parseString(json, pos);
        if (c == 't' || c == 'f') return parseBoolean(json, pos);
        if (c == 'n') return parseNull(json, pos);
        if (c == '-' || isdigit(static_cast<unsigned char>(c))) return parseNumber(json, pos);
        throw std::runtime_error(std::string("Invalid JSON value at pos ") + std::to_string(pos));
    }

    static std::unique_ptr<JsonBase> parseObject(const std::string& json, size_t& pos) {
        ++pos; // skip '{'
        skipSpaces(json, pos);
        JsonObject::Members members;
        if (pos < json.size() && json[pos] == '}') { ++pos; return std::make_unique<JsonObject>(std::move(members)); }
        while (true) {
            skipSpaces(json, pos);
            if (pos >= json.size() || json[pos] != '"') throw std::runtime_error("Expected string key in object");
            auto keyNode = parseString(json, pos);
            std::string key = keyNode->asString();
            skipSpaces(json, pos);
            if (pos >= json.size() || json[pos] != ':') throw std::runtime_error("Expected ':' after key");
            ++pos;
            skipSpaces(json, pos);
            auto val = parseValue(json, pos);
            members.emplace_back(std::move(key), std::move(val));
            skipSpaces(json, pos);
            if (pos >= json.size()) throw std::runtime_error("Unexpected end in object");
            if (json[pos] == '}') { ++pos; break; }
            if (json[pos] == ',') { ++pos; continue; }
            throw std::runtime_error("Expected ',' or '}' in object");
        }
        return std::make_unique<JsonObject>(std::move(members));
    }

    static std::unique_ptr<JsonBase> parseArray(const std::string& json, size_t& pos) {
        ++pos; // skip '['
        skipSpaces(json, pos);
        JsonArray::Elements elems;
        if (pos < json.size() && json[pos] == ']') { ++pos; return std::make_unique<JsonArray>(std::move(elems)); }
        while (true) {
            skipSpaces(json, pos);
            auto v = parseValue(json, pos);
            elems.push_back(std::move(v));
            skipSpaces(json, pos);
            if (pos >= json.size()) throw std::runtime_error("Unexpected end in array");
            if (json[pos] == ']') { ++pos; break; }
            if (json[pos] == ',') { ++pos; continue; }
            throw std::runtime_error("Expected ',' or ']' in array");
        }
        return std::make_unique<JsonArray>(std::move(elems));
    }

    static std::unique_ptr<JsonBase> parseString(const std::string& json, size_t& pos) {
        ++pos; // skip '"'
        std::string out;
        while (pos < json.size()) {
            char c = json[pos++];
            if (c == '"') break;
            if (c == '\\') {
                if (pos >= json.size()) throw std::runtime_error("Bad escape in string");
                char esc = json[pos++];
                switch (esc) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    // unicode \uXXXX not fully supported here
                    default: out.push_back(esc); break;
                }
            } else {
                out.push_back(c);
            }
        }
        return std::make_unique<JsonString>(std::move(out));
    }

    static std::unique_ptr<JsonBase> parseNumber(const std::string& json, size_t& pos) {
        size_t start = pos;
        if (json[pos] == '-') ++pos;
        while (pos < json.size() && isdigit(static_cast<unsigned char>(json[pos]))) ++pos;
        if (pos < json.size() && json[pos] == '.') {
            ++pos;
            while (pos < json.size() && isdigit(static_cast<unsigned char>(json[pos]))) ++pos;
        }
        // exponent not strictly required but supported
        if (pos < json.size() && (json[pos] == 'e' || json[pos] == 'E')) {
            ++pos;
            if (pos < json.size() && (json[pos] == '+' || json[pos] == '-')) ++pos;
            while (pos < json.size() && isdigit(static_cast<unsigned char>(json[pos]))) ++pos;
        }
        double val = 0.0;
        try {
            val = std::stod(json.substr(start, pos - start));
        } catch (...) {
            throw std::runtime_error("Invalid number");
        }
        return std::make_unique<JsonNumber>(val);
    }

    static std::unique_ptr<JsonBase> parseBoolean(const std::string& json, size_t& pos) {
        if (json.compare(pos, 4, "true") == 0) { pos += 4; return std::make_unique<JsonBoolean>(true); }
        if (json.compare(pos, 5, "false") == 0) { pos += 5; return std::make_unique<JsonBoolean>(false); }
        throw std::runtime_error("Invalid boolean");
    }

    static std::unique_ptr<JsonBase> parseNull(const std::string& json, size_t& pos) {
        if (json.compare(pos, 4, "null") == 0) { pos += 4; return std::make_unique<JsonNull>(); }
        throw std::runtime_error("Invalid null");
    }
};

} // namespace hspd

#endif // JSON_HPP
