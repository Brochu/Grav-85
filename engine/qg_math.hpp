#pragma once
#include "shared_types.hpp"

enum class direction : u8 {
    UP = 0,
    RIGHT = 1,
    DOWN = 2,
    LEFT = 3,
    COUNT = 4
};

struct ivec2 {
    i32 x;
    i32 y;
};

struct vec2 {
    f32 x;
    f32 y;
};

// Arithmetic
inline ivec2 operator+(ivec2 a, ivec2 b) { return {a.x + b.x, a.y + b.y}; }
inline ivec2 operator-(ivec2 a, ivec2 b) { return {a.x - b.x, a.y - b.y}; }
inline ivec2 operator-(ivec2 v) { return {-v.x, -v.y}; }
inline ivec2 operator*(ivec2 v, i32 s) { return {v.x * s, v.y * s}; }
inline ivec2 operator*(i32 s, ivec2 v) { return {v.x * s, v.y * s}; }

// Comparison
inline bool operator==(ivec2 a, ivec2 b) { return a.x == b.x && a.y == b.y; }
inline bool operator!=(ivec2 a, ivec2 b) { return !(a == b); }

// Dot product
inline i32 ivec2_dot(ivec2 a, ivec2 b) { return a.x * b.x + a.y * b.y; }

// Conversion
inline ivec2 to_ivec2(vec2 v) { return {(i32)v.x, (i32)v.y}; }

// Manhattan distance between two points
inline i32 ivec2_manhattan(ivec2 a, ivec2 b) {
    i32 dx = a.x - b.x;
    i32 dy = a.y - b.y;
    return (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
}

// Direction constants
constexpr ivec2 ivec2_zero  = { 0,  0};
constexpr ivec2 ivec2_up    = { 0, -1};
constexpr ivec2 ivec2_right = { 1,  0};
constexpr ivec2 ivec2_down  = { 0,  1};
constexpr ivec2 ivec2_left  = {-1,  0};

// Indexed by direction enum
constexpr ivec2 direction_vectors[] = {
    ivec2_up,
    ivec2_right,
    ivec2_down,
    ivec2_left,
};

// Arithmetic
inline vec2 operator+(vec2 a, vec2 b) { return {a.x + b.x, a.y + b.y}; }
inline vec2 operator-(vec2 a, vec2 b) { return {a.x - b.x, a.y - b.y}; }
inline vec2 operator-(vec2 v) { return {-v.x, -v.y}; }
inline vec2 operator*(vec2 v, f32 s) { return {v.x * s, v.y * s}; }
inline vec2 operator*(f32 s, vec2 v) { return {v.x * s, v.y * s}; }

// Comparison (approximate, using squared distance)
#define EPSILON 0.0001f
inline bool operator==(vec2 a, vec2 b) {
    f32 dx = a.x - b.x, dy = a.y - b.y;
    return dx*dx + dy*dy < EPSILON * EPSILON;
}
inline bool operator!=(vec2 a, vec2 b) { return !(a == b); }

// Dot product
inline f32 vec2_dot(vec2 a, vec2 b) { return a.x * b.x + a.y * b.y; }

// Conversion
inline vec2 to_vec2(ivec2 v) { return {(f32)v.x, (f32)v.y}; }

// Constants
constexpr vec2 vec2_zero = {0.0f, 0.0f};

// Scalar move_toward: moves `current` toward `target` by at most `max_step`
inline f32 math_move_toward(f32 current, f32 target, f32 max_step) {
    f32 diff = target - current;
    if (diff > max_step) return current + max_step;
    if (diff < -max_step) return current - max_step;
    return target;
}
