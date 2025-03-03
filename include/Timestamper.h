#pragma once

#include <chrono>
#include <iostream>
#include <list>


class Timestamper {
    const std::chrono::milliseconds ini_tm;
    std::chrono::milliseconds tm;
    std::string name;
    std::list<Timestamper> subs;

    static std::chrono::milliseconds get_tm() {
        return duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        );
    }

public:
    explicit Timestamper(std::string name) : ini_tm(get_tm()), tm(ini_tm), name(std::move(name)) {
        // std::cout << "[" << this->name << "=0]" << '\n';
    }

    ~Timestamper() {
        std::cout << "[" << name << "=" << get_tm() - ini_tm << "]" << '\n';
    }

    std::string stamp() {
        if (!subs.empty()) {
            auto t = --subs.end();
            return t->stamp();
        }
        std::stringstream ss;
        auto newt = get_tm();
        ss << "[" << name << "+" << newt - tm << "] ";
        tm = newt;
        return ss.str();
    }

    void sub(const std::string &name) {
        subs.emplace_back(name);
    }

    void out() {
        subs.pop_back();
        tm = get_tm();
    }
};