#pragma once

#include <chrono>
#include <iostream>
#include <list>


class Timestamper {
    struct timestamper_elem_t {
        const std::chrono::milliseconds ini_tm;
        std::chrono::milliseconds tm;
        std::string name;
    };

    std::list<timestamper_elem_t> elems;

    static std::chrono::milliseconds get_tm() {
        return duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        );
    }

    static void put_tm_formatted(std::ostream &ss, std::chrono::milliseconds ms) {
        using namespace std::chrono;
        if (ms > hours(1)) {
            ss << std::format("{:%Hh:%Mm:%Ss}", floor<seconds>(ms));
        } else if (ms > minutes(1)) {
            ss << std::format("{:%Mm:%Ss}", floor<seconds>(ms));
        } else if (ms > seconds(10)) {
            ss << std::format("{:%Ss}", floor<seconds>(ms));
        } else {
            ss << ms;
        }
    }

    void root_out_message() {
        auto rm_el = *--elems.end();
        std::cout << "[" << rm_el.name << "=";
        put_tm_formatted(std::cout, get_tm() - rm_el.ini_tm);
        std::cout << "]" << std::endl;
    }

public:
    explicit Timestamper(const std::string &name) {
        sub(name);
    }

    ~Timestamper() {
        while (elems.size() != 1) out();
        root_out_message();
        elems.pop_back();
    }

    std::string stamp() {
        if (elems.empty()) {
            throw std::runtime_error("empty timestamper context stack");
        }
        auto &el = *--elems.end();

        std::stringstream ss;
        auto newt = get_tm();
        ss << "[" << el.name << "+";
        put_tm_formatted(ss, newt - el.tm);
        ss << "] ";

        el.tm = newt;
        return ss.str();
    }

    void sub(const std::string &name) {
        auto cur_tm = get_tm();
        elems.emplace_back(cur_tm, cur_tm, name);
    }

    void dry_out() {
        if (elems.empty()) {
            throw std::runtime_error("empty timestamper context stack");
        }
        root_out_message();
    }

    void out() {
        if (elems.size() < 2) {
            throw std::runtime_error("empty timestamper context stack");
        }
        auto rm_el = *--elems.end();
        elems.pop_back();
        auto &el = *--elems.end();
        std::cout << "[" << el.name << "+";
        std::cout << rm_el.name << "(";
        put_tm_formatted(std::cout, get_tm() - rm_el.ini_tm);
        std::cout << ")]" << std::endl;
        stamp();
    }
};
