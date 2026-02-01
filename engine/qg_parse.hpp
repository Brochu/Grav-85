#pragma once
#include "shared_types.hpp"

#include <cstring>

#define SV_FMT "%.*s"
#define SV_ARG(sv) (int)sv.len, sv.ptr

struct strview {
    const char* ptr;
    size_t len;
};

static inline strview sv(const char *s) {
    return { s, strlen(s) };
}

strview sv_find(strview haystack, const char *needle);

u64 sv_split(strview str, const char *delim, strview *out_elems, u64 max_elems);

bool sv_split_once(strview str, const char *delim, strview* first, strview* second);
