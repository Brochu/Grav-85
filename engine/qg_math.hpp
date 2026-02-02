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

// Arithmetic
inline ivec2 operator+(ivec2 a, ivec2 b) { return {a.x + b.x, a.y + b.y}; }
inline ivec2 operator-(ivec2 a, ivec2 b) { return {a.x - b.x, a.y - b.y}; }
inline ivec2 operator-(ivec2 v) { return {-v.x, -v.y}; }
inline ivec2 operator*(ivec2 v, i32 s) { return {v.x * s, v.y * s}; }
inline ivec2 operator*(i32 s, ivec2 v) { return {v.x * s, v.y * s}; }

// Comparison
inline bool operator==(ivec2 a, ivec2 b) { return a.x == b.x && a.y == b.y; }
inline bool operator!=(ivec2 a, ivec2 b) { return !(a == b); }

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

// Manhattan distance between two points
inline i32 ivec2_manhattan(ivec2 a, ivec2 b) {
    i32 dx = a.x - b.x;
    i32 dy = a.y - b.y;
    return (dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy);
}
