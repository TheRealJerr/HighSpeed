#include <iostream>
#include <protocol/Json.hpp>




using namespace hspd;

int main() {
    // create root as object
    
    JsonValue root;

    root["name"] = "Jerry";
    root["age"] = 21;
    root["pi"] = 3.14159;
    root["alive"] = true;
    root["nothing"] = nullptr;

    root["user"]["id"] = 123;
    root["user"]["meta"]["nick"] = "ruijie";

    root["arr"][0] = "a";
    root["arr"][3] = 999; // auto expand and fill with nulls
    std::cout << root.dump(2) << std::endl;

    // parse example
    std::string json = R"({"x":1,"y":[true, false, "hi"]})";
    auto parsed = JsonParser::parse(json);
    JsonValue p(&parsed);
    std::cout << p.dump(2) << std::endl;

    return 0;
}
