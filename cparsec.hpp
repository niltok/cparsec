#pragma once

#include <string>
#include <list>
#include <memory>
#include <functional>
#include <iostream>

struct ParseState
{
    unsigned long long pos;
    std::shared_ptr<std::string> s;
    ParseState(const ParseState &s): pos(s.pos), s(s.s) {}
    explicit ParseState(const std::string &s): pos(0), s(std::make_shared<std::string>(s)) {}
    ParseState(unsigned long long p, std::shared_ptr<std::string> s): pos(p), s(s) {}
};

template<typename T>
struct ParseResult {
    bool success;
    union {
        std::list<std::string> error;
        T result;
    };
    ParseState state;
    ParseResult(T r, ParseState s): result(r), state(s), success(true) {}
    ParseResult(ParseState s, std::list<std::string> e): state(s), error(e), success(false) {}
    ParseResult(const ParseResult &r): state(r.state), success(r.success) {
        if (r.success) result = r.result;
        else error = r.error;
    }
    ~ParseResult() {
        if (!success) error.~list();
    }
    bool operator==(const ParseResult &s) const {
        if (success != s.success) return false;
        if (success) return result == s.result;
        else return error == s.error;
    }
};

template<typename T>
ParseResult<T> success(ParseState s, T r) {
    return ParseResult<T>(r, s);
}

template<typename T>
using Parser = std::function<ParseResult<T>(ParseState)>;

template<typename T>
ParseResult<T> parse(Parser<T> p, std::string s) {
    return p(ParseState(s));
}

template<typename A, typename B>
Parser<B> mapP(Parser<A> p, std::function<B(A)> f) {
    return [=] (ParseState s) {
        ParseResult<A> r = p(s);
        if (!r.success) return ParseResult<B>(r.state, r.error);
        return success(r.state, f(r.result));
    };
}

template<typename T>
Parser<T> orP(Parser<T> p) {
    return p;
}

template<typename T, typename... Args>
Parser<T> orP(Parser<T> p, Args... ps) {
    return [=] (ParseState s) -> ParseResult<T> {
        ParseResult<T> r = p(s);
        if (!r.success) return orP(ps...)(s);
        return r;
    };
}

template<typename T>
Parser<T> pureP(T x) {
    return [=] (ParseState s) {
        return success(s, x);
    };
}

template<typename A, typename B, typename C>
Parser<C> andP(Parser<A> pa, Parser<B> pb, std::function<C(A, B)> f) {
    return [=] (ParseState s) {
        ParseResult<A> ra = pa(s);
        if (!ra.success) return ParseResult<C>(ra.state, ra.error);
        ParseResult<B> rb = pb(ra.state);
        if (!rb.success) return ParseResult<C>(rb.state, rb.error);
        return success(rb.state, f(ra.result, rb.result));
    };
}

template<typename A, typename B>
Parser<A> leftP(Parser<A> pa, Parser<B> pb) {
    return andP<A, B, A>(pa, pb, [] (A a, B b) { return a; });
}

template<typename A, typename B>
Parser<B> rightP(Parser<A> pa, Parser<B> pb) {
    return andP<A, B, B>(pa, pb, [] (A a, B b) { return b; });
}

template<typename T>
Parser<std::list<T>> manyP(Parser<T> p) {
    return [=] (ParseState s) {
        ParseResult<T> r = p(s);
        if (!r.success) return success(s, std::list<T>());
        ParseResult<std::list<T>> rs = manyP(p)(r.state);
        rs.result.push_front(r.result);
        return success(rs.state, rs.result);
    };
}

template<typename T>
Parser<std::list<T>> someP(Parser<T> p) {
    return andP<T, std::list<T>, std::list<T>>(p, manyP(p), [] (T r, std::list<T> rs) {
        rs.push_front(r);
        return rs;
    });
}

Parser<char> idP = [] (ParseState s) {
    if (s.pos == s.s->size()) 
        return ParseResult<char>(s, std::list<std::string>{"end of file"});
    else return success(ParseState(s.pos + 1, s.s), s.s->at(s.pos));
};

Parser<bool> eofP = [] (ParseState s) {
    if (s.pos == s.s->size()) return success(s, true);
    else return ParseResult<bool>(s, std::list<std::string>{"expect end of file"});
};

Parser<char> predP(std::function<bool(char)> f) {
    return [=] (ParseState s) {
        ParseResult<char> r = idP(s);
        if (!r.success) return ParseResult<char>(r.state, r.error);
        if (!f(r.result)) 
            return ParseResult<char>(r.state, std::list<std::string>{"unexpect " + std::string{r.result}});
        return r;
    };
}

Parser<char> charP(char c) {
    return predP([=] (char x) { return c == x; });
}

Parser<char> oneOfP(std::string s) {
    return predP([=] (char c) {
        bool t = false;
        for (int i = 0; i < s.size(); i++) t |= c == s[i];
        return t;
    });
}

Parser<std::string> stringP(std::string str) {
    return [=] (ParseState s) {
        if (str == "") return success(s, std::string(""));
        ParseResult<char> r = charP(str.front())(s);
        if (!r.success) return ParseResult<std::string>(r.state, r.error);
        ParseResult<std::string> rs = stringP(std::string(str.begin() + 1, str.end()))(r.state);
        if (!rs.success) return rs;
        return success(rs.state, str);
    };
}

Parser<char> space = oneOfP(" \n\t");
Parser<std::list<char>> spaces = manyP(space);
Parser<int> digitP = mapP<char, int>(predP([] (char c) { return '0' <= c && c <= '9'; }), 
                                     [] (char c) { return c - '0'; });
Parser<int> natP = mapP<std::list<int>, int>(someP(digitP), [] (std::list<int> ns) {
    int n = 0;
    for (int i : ns) n = n * 10 + i;
    return n;
});
Parser<int> intP = orP(mapP<int, int>(rightP(charP('-'), natP), [] (int x) { return -x; }), natP);
Parser<double> doubleP = orP(andP<int, double, double>(intP, rightP<char, double>(charP('.'), 
    mapP<std::list<int>, double>(someP(digitP), [] (std::list<int> ns) {
        double ans = 0, base = 0.1;
        for (int i : ns) {
            ans += i * base;
            base /= 10;
        }
        return ans;
    })), [] (int x, double y) { return x + y; }), mapP<int, double>(intP, [] (int x) { return x; }));

template<typename A, typename B, typename C>
Parser<C> betweenP(Parser<A> lp, Parser<B> rp, Parser<C> p) {
    return rightP<A, C>(lp, leftP<C, B>(p, rp));
}

template<typename T>
Parser<T> trimP(Parser<T> p) {
    return rightP<std::list<char>, T>(spaces, leftP<T, std::list<char>>(p, spaces));
}

template<typename T>
Parser<T> lazyP(std::function<Parser<T>()> f) {
    return [=] (ParseState s) { return f()(s); };
}
