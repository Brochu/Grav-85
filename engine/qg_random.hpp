#pragma once
#include "shared_types.hpp"

void rand_seed(i64 seed);

f32 rand_float01();

i32 rand_int(i32 max_val);
i32 rand_int_min(i32 min_val, i32 max_val);

template<class T>
i32 rand_weighted_index(T *weights, i32 num_items) {
    i64 sum = 0;
    for (int i = 0; i < num_items; i++) {
        sum += weights[i];
    }
    if (sum <= 0) return 0;

    i64 target = (int)(rand_float01() * sum) + 1;

    i32 current = 0;
    while (current < num_items && target > weights[current]) {
        target -= weights[current];
        current++;
    }
    return current < num_items ? current : num_items - 1;
}

template<class T>
i32 rand_weighted_index(f32 roll, T *weights, i32 num_items) {
    i64 sum = 0;
    for (int i = 0; i < num_items; i++) {
        sum += weights[i];
    }
    if (sum <= 0) return 0;

    i64 target = (int)(roll * sum) + 1;

    i32 current = 0;
    while (current < num_items && target > weights[current]) {
        target -= weights[current];
        current++;
    }
    return current < num_items ? current : num_items - 1;
}
