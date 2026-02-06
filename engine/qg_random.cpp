#include "qg_random.hpp"
#include <cassert>
#include <random>

thread_local std::mt19937_64 g_rand_eng;
thread_local std::uniform_real_distribution<f32> g_rand_dist;
thread_local std::normal_distribution<f32> g_age_dist(30.0, 16.0);

void rand_seed(i64 seed) {
    g_rand_eng.seed(seed);
}

f32 rand_float01() {
    return g_rand_dist(g_rand_eng);
}

i32 rand_int(i32 max_val) {
    assert(max_val > 0);

    return (i32)(rand_float01() * max_val);
}
i32 rand_int_min(i32 min_val, i32 max_val) {
    assert(max_val > 0);
    assert(min_val < max_val);

    return min_val + (i32)(rand_float01() * (max_val - min_val));
}
