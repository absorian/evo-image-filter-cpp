#pragma once

#include <functional>
#include <future>

class Parallelizer {
    std::vector<std::future<void> > futs;

public:
    const int threads_count;

    explicit Parallelizer(int threads_count) : futs(threads_count), threads_count(threads_count) {
    }

    template<typename Ct>
    void call(Ct &storage, int element_limit,
              const std::function<void(typename Ct::iterator, typename Ct::iterator)> &fn) {
        const int el_per_thread = element_limit / threads_count;
        for (int i = 0; i < threads_count; ++i) {
            auto rbound = i + 1 == threads_count ? storage.end() : storage.begin() + el_per_thread * (i + 1);
            futs[i] = std::async(std::launch::async, fn,
                                 storage.begin() + el_per_thread * i, rbound);
        }
        for (int i = 0; i < threads_count; ++i) {
            futs[i].wait();
        }
    }

    template<typename Ct>
    void call(typename Ct::iterator begin, int element_limit,
              const std::function<void(typename Ct::iterator, typename Ct::iterator)> &fn) {
        const int el_per_thread = element_limit / threads_count;
        const int el_extra_count = element_limit % threads_count;
        for (int i = 0; i < threads_count; ++i) {
            auto rbound = begin + el_per_thread * (i + 1);
            if (i + 1 == threads_count) rbound += el_extra_count;

            futs[i] = std::async(std::launch::async, fn,
                                 begin + el_per_thread * i, rbound);
        }
        for (int i = 0; i < threads_count; ++i) {
            futs[i].wait();
        }
    }
};
