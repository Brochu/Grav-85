#include "qg_parse.hpp"

strview sv_find(strview haystack, const char *needle) {
    strview n = sv(needle);
    if (n.len == 0 || n.len > haystack.len) {
        return { nullptr, 0 };
    }

    for (size_t i = 0; i <= haystack.len - n.len; i++) {
        if (memcmp(haystack.ptr + i, n.ptr, n.len) == 0) {
            return { haystack.ptr + i, n.len };
        }
    }

    return { nullptr, 0 };
}

u64 sv_split(strview str, const char *delim, strview *out_elems, u64 max_elems) {
    u64 pos = 0;
    u64 num_elems = 0;

    while (pos < str.len) {
        if (num_elems >= max_elems) return 0; // Found too manu delims

        strview start{ str.ptr + pos, str.len - pos };
        size_t end = (sv_find(start, delim).ptr - str.ptr);

        if (end >= str.len) {
            out_elems[num_elems++] = { str.ptr + pos, str.len - pos };
            break;
        }

        out_elems[num_elems++] = { str.ptr + pos, end - pos };
        pos = end + strlen(delim); // skip over the whole delimiter
    }

    return num_elems;
}

bool sv_split_once(strview str, const char *delim, strview* first, strview* second) {
    strview d = sv(delim);
    strview found = sv_find(str, delim);
    if (found.ptr == nullptr) {
        return false;
    }

    size_t pos = found.ptr - str.ptr;

    *first = { str.ptr, pos };
    *second = { found.ptr + d.len, str.len - pos - d.len };
    return true;
}
