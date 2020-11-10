#pragma once

#include "cparsec.hpp"
#include <string>
#include <list>
#include <map>
#include <utility>
#include <variant>

struct JsonValue {
    enum JsonType {
        JSON_NULL,
        JSON_BOOL,
        JSON_NUM,
        JSON_STRING,
        JSON_ARRAY,
        JSON_OBJECT
    };
    JsonType type;
    union {
        bool boolValue;
        double numValue;
        std::string strValue;
        std::list<JsonValue> arrayValue;
        std::map<std::string, JsonValue> objectValue;
    };
    JsonValue(): type(JSON_NULL) {}
    JsonValue(bool v): type(JSON_BOOL), boolValue(v) {}
    JsonValue(double v): type(JSON_NUM), numValue(v) {}
    JsonValue(std::string v): type(JSON_STRING), strValue(v) {}
    JsonValue(std::list<JsonValue> v): type(JSON_ARRAY), arrayValue(v) {}
    JsonValue(std::map<std::string, JsonValue> v): type(JSON_OBJECT), objectValue(v) {}
    JsonValue(const JsonValue &v): type(v.type) {
        switch (v.type) {
            case JSON_NULL: break;
            case JSON_BOOL: boolValue = v.boolValue; break;
            case JSON_NUM: numValue = v.numValue; break;
            case JSON_STRING: new (&strValue) std::string(v.strValue); break;
            case JSON_ARRAY: new (&arrayValue) std::list<JsonValue>(v.arrayValue); break;
            case JSON_OBJECT: new (&objectValue) std::map<std::string, JsonValue>(v.objectValue); break;
        }
    }
    JsonValue &operator=(const JsonValue &v) {
        type = v.type;
        switch (v.type) {
            case JSON_NULL: break;
            case JSON_BOOL: boolValue = v.boolValue; break;
            case JSON_NUM: numValue = v.numValue; break;
            case JSON_STRING: new (&strValue) std::string(v.strValue); break;
            case JSON_ARRAY: new (&arrayValue) std::list<JsonValue>(v.arrayValue); break;
            case JSON_OBJECT: new (&objectValue) std::map<std::string, JsonValue>(v.objectValue); break;
        }
        return *this;
    }
    ~JsonValue() {
        switch (type) {
            case JSON_STRING: strValue.~basic_string(); break;
            case JSON_ARRAY: arrayValue.~list(); break;
            case JSON_OBJECT: objectValue.~map(); break;
            default: break;
        }
    }
    bool operator==(const JsonValue &v) const {
        if (type != v.type) return false;
        switch (type) {
            case JSON_NULL: return true;
            case JSON_BOOL: return boolValue == v.boolValue;
            case JSON_NUM: return numValue == v.numValue;
            case JSON_STRING: return strValue == v.strValue;
            case JSON_ARRAY: return arrayValue == v.arrayValue;
            case JSON_OBJECT: return objectValue == v.objectValue;
        }
        return false;
    }
};

std::ostream &operator<<(std::ostream &os, const JsonValue &js) {
    switch (js.type) {
        case JsonValue::JSON_NULL: os << "null"; break;
        case JsonValue::JSON_BOOL: os << (js.boolValue ? "true" : "false"); break;
        case JsonValue::JSON_NUM: os << js.numValue; break;
        case JsonValue::JSON_STRING: os << js.strValue; break;
        case JsonValue::JSON_ARRAY: {
            os << "[";
            for (const JsonValue &i : js.arrayValue) os << i << ", ";
            os << "]";
            break;
        }
        case JsonValue::JSON_OBJECT: {
            os << "{";
            for (const std::pair<std::string, JsonValue> &i : js.objectValue)
                os << '"' << i.first << '"' << " : " << i.second << ", ";
            os << "}";
            break;
        }
    }
    return os;
}

Parser<JsonValue> nullP();
Parser<JsonValue> boolP();
Parser<JsonValue> numP();
Parser<JsonValue> strP();
Parser<JsonValue> arrayP();
Parser<JsonValue> objectP();
Parser<JsonValue> jsonP();

Parser<JsonValue> nullP() {
    return trimP(mapP<std::string, JsonValue>(stringP("null"), [] (std::string s) { return JsonValue(); }));
}

Parser<JsonValue> boolP() {
    return trimP(orP(mapP<std::string, JsonValue>(stringP("true") , [] (std::string s) { return JsonValue(true) ; }),
                     mapP<std::string, JsonValue>(stringP("false"), [] (std::string s) { return JsonValue(false); })));
}

Parser<JsonValue> numP() {
    return trimP(mapP<double, JsonValue>(doubleP, [] (double x) { return JsonValue(x); }));
}

Parser<char> escapeP = mapP<char, char>(idP, [] (char c) {
    switch (c) {
        case '0': return '\0';
        case 'b': return '\b';
        case 't': return '\t';
        case 'n': return '\n';
        case 'v': return '\v';
        case 'f': return '\f';
        case 'r': return '\r';
    }
    return c;
});

Parser<std::string> escapeStrP = trimP(betweenP(charP('"'), charP('"'),
    mapP<std::list<char>, std::string>(manyP(orP(rightP(charP('\\'), escapeP), predP([] (char c) { return c != '"'; }))), 
    [] (std::list<char> cs) {
        std::string s = "";
        for (char x : cs) s.push_back(x);
        return s;
    })));

Parser<JsonValue> strP() {
    return mapP<std::string, JsonValue>(escapeStrP, [] (std::string s) { return JsonValue(s); });
}

Parser<JsonValue> arrayP() {
    Parser<JsonValue> p = orP(andP<JsonValue, std::list<JsonValue>, JsonValue>(lazyP<JsonValue>(jsonP), 
        manyP(rightP(charP(','), lazyP<JsonValue>(jsonP))), [] (JsonValue x, std::list<JsonValue> xs) {
            xs.push_front(x);
            return JsonValue(xs);
        }), pureP(JsonValue(std::list<JsonValue>())));
    return trimP(betweenP(charP('['), charP(']'), p));
}

Parser<JsonValue> objectP() {
    using kv = std::pair<std::string, JsonValue>;
    Parser<kv> itemP = andP<std::string, JsonValue, kv>
        (escapeStrP, rightP(charP(':'), lazyP<JsonValue>(jsonP)), std::make_pair<std::string, JsonValue>);
    Parser<JsonValue> p = orP(andP<kv, std::list<kv>, JsonValue>(itemP, 
        manyP(rightP(charP(','), itemP)), [] (kv x, std::list<kv> xs) {
            xs.push_front(x);
            std::map<std::string, JsonValue> m = std::map<std::string, JsonValue>();
            for (kv i : xs) m[i.first] = i.second;
            return JsonValue(m);
        }), pureP(JsonValue(std::map<std::string, JsonValue>())));
    return trimP(betweenP(charP('{'), charP('}'), p));
}

Parser<JsonValue> jsonP() {
    return orP(nullP(), boolP(), numP(), strP(), lazyP<JsonValue>(arrayP), lazyP<JsonValue>(objectP));
}
