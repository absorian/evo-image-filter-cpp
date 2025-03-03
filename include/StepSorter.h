#pragma once

#include <functional>

class StepSorter {
public:
    const int keep_objs_count;
    const int first_batch_size;
    const int next_batch_size;
    const int storage_size;

    StepSorter(int keep_objs_count, int sortout_overhead_percent)
        : keep_objs_count(keep_objs_count),
          first_batch_size((100 + sortout_overhead_percent) * 0.01 * keep_objs_count),
          next_batch_size(sortout_overhead_percent * 0.01 * keep_objs_count),
          storage_size(first_batch_size) {
    }

    template<typename Ct>
    void call(Ct &storage, int element_limit,
              const std::function<void(typename Ct::iterator, typename Ct::iterator, int)> &fn,
              std::function<bool(const typename Ct::value_type &, const typename Ct::value_type &)> comparator) {
        int batch_i = 0;
        int el_count = 0;
        while (el_count < element_limit) {
            int cur_batch_size = !batch_i ? first_batch_size : next_batch_size;
            int cur_batch_offt = !batch_i ? 0 : keep_objs_count;
            fn(storage.begin() + cur_batch_offt, storage.begin() + cur_batch_size, cur_batch_size);
            el_count += cur_batch_size;

            std::sort(storage.begin(), storage.begin() + cur_batch_size, comparator);
            batch_i++;
        }
    }
};
