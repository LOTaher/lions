/*  lt_strings.h - Single file library for my strings implementation
    Copyright (C) 2026 splatte.dev

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>. */


#ifndef LT_STRINGS_H
#define LT_STRINGS_H

#include <string.h>

#define LT_ARENA_IMPLEMENTATION
#include "lt_arena.h"
#include "lt_base.h"

// NOTE(laith): utf-8 string, windows works with utf-16 strings
typedef struct {
    u8* str;
    u64 length;
} string8;

#define str8_lit(s) (string8){ (u8*)(s), sizeof(s) - 1 }
#define str8_fmt(s) (int)(s).length, (s).str

// TODO(laith): add str8_split and a string8 array data type

#if defined(LT_STRING_IMPLEMENTATION)
string8 str8_substring(string8 str, u64 start, u64 end) {
    end = MIN(end, str.length);
    start = MIN(start, end);

    return (string8){ str.str + start, end - start };
}

string8 str8_cstring(char *str) {
    return (string8){ (u8*)str, strlen(str) };
}

s8 str8_compare(string8 str1, string8 str2) {
    if (str1.length != str2.length) return -1;

    return memcmp(str1.str, str2.str, str1.length) == 0 ? 1 : -1;
}

s8 str8_contains(string8 str, string8 substr) {
    if (substr.length > str.length) return -1;

    for (u64 i = 0; i < str.length; i++) {
        string8 sub = str8_substring(str, i, substr.length + i);

        if (str8_compare(sub, substr)) return 1;
    }

    return -1;
}

string8 str8_copy(string8 str, mem_arena* arena) {
    u8* bytes = (u8*)arena_push(arena, str.length);

    memcpy(bytes, &str, str.length);

    return (string8){ bytes, str.length };
}

string8 str8_concat(string8 str1, string8 str2, mem_arena* arena) {
    u8* bytes = (u8*)arena_push(arena, str1.length + str2.length);

    memcpy(bytes, str1.str, str1.length);
    memcpy(bytes + str1.length, str2.str, str2.length);

    return (string8){ bytes, str1.length + str2.length };
}

#endif // LT_STRING_IMPLEMENTATION
#endif // LT_STRINGS_H
