#pragma once
#include "qg_shared.hpp"
#include "qg_shared_types.hpp"

#include <cstring>

strview sv_find(strview haystack, const char *needle);
u64 sv_split(strview str, const char *delim, strview *out_elems, u64 max_elems);
bool sv_split_once(strview str, const char *delim, strview* first, strview* second);
