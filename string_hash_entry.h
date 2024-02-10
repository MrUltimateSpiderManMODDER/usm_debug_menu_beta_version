#pragma once

#include "mstring.h"
#include "string_hash.h"

struct string_hash_entry {
    string_hash field_0;
    mString field_4;

    string_hash_entry() = default;

    string_hash_entry(const char *a2, const string_hash *a3);

    mString sub_50DBC0(const char *a3);
};
