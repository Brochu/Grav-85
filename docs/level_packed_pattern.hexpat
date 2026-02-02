// Enter your pattern code here and click on the Play button
// below to execute your code. The results will then be visible
// in the Pattern Data view.
// 
// More information can be found in the documentation.
// 
// 
// Simple example:
// 
// import std.io;
// 
// struct Pattern {
//     u32 int;
//     float floating_point;
// };
// 
// Pattern my_pattern @ 0x00;
// std::print("0x{:08X}", my_pattern.int);

import std.io;

struct level {
    char width;
    char height;
    char start_gravity;
    
    char colors_len;
    u64 colors;
    
    u64 spaces_hi;
    u64 spaces_lo;
};

level lvl @ 0x00;