#pragma once

#include "mstring.h"

struct mission_manager_script_data {
    mString field_0;
    int field_10[39];

    bool uses_script_stack;
    int filed_B0;
    int field_B4;
    mString field_B8;
    mString field_C8;
    mString field_D8;
};

void copy(const mission_manager_script_data& a2)
{
    CDECL_CALL(0x005E98A0, &a2);
}