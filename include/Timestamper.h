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

    void out_formatted(std::ostream &ss, std::chrono::milliseconds ms) {
        if (ms > std::chrono::milliseconds(10000))
            ss << std::chrono::duration_cast<std::chrono::seconds>(ms);
        else
            ss << ms;
    }

    void _dry_out() {
        std::cout << "[" << name << "=";
        out_formatted(std::cout, get_tm() - ini_tm);
        std::cout << "]" << '\n';
    }

public:
    explicit Timestamper(std::string name) : ini_tm(get_tm()), tm(ini_tm), name(std::move(name)) {
        // std::cout << "[" << this->name << "=0]" << '\n';
    }

    ~Timestamper() {
        _dry_out();
    }

    std::string stamp() {
        if (!subs.empty()) {
            auto t = --subs.end();
            return t->stamp();
        }
        std::stringstream ss;
        auto newt = get_tm();
        ss << "[" << name << "+";
        out_formatted(ss, newt - tm);
        ss << "] ";


        tm = newt;
        return ss.str();
    }

    void sub(const std::string &name) {
        subs.emplace_back(name);
    }

    void dry_out() {
        if (!subs.empty()) {
            auto t = --subs.end();
            t->_dry_out();
        } else _dry_out();
    }

    void out() {
        subs.pop_back();
        tm = get_tm();
    }
};
