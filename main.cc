#include "cparsec.hpp"
#include <cassert>
#include <memory>
#include <iostream>
#include <list>
#include <map>
#include <utility>
#include "jsonp.hpp"

int main() {
    assert(parse(idP, "a").result == 'a');
    assert((parse(someP(oneOfP("a")), "aaa").result == std::list<char>{'a', 'a', 'a'}));
    assert(parse(intP, "-25").result == -25);
    assert(parse(doubleP, "12.25").result == 12.25);
    assert(parse(jsonP(), "null").result == JsonValue());
    assert(parse(jsonP(), "true").result == JsonValue(true));
    assert(parse(jsonP(), "[1, 2]").result == JsonValue(std::list<JsonValue>{JsonValue(1.0), JsonValue(2.0)}));
    assert(parse(jsonP(), "{\"xyz\": 2, \"abc\": 1}").result == JsonValue(std::map<std::string, JsonValue>{
        {"abc", JsonValue(1.0)},
        {"xyz", JsonValue(2.0)}}));
}