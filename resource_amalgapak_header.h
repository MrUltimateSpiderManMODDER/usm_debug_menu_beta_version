#pragma once


#include "resource_versions.h"
#include "variable.h"

struct mString;

struct resource_amalgapak_header {
    resource_versions field_0;
    int field_14;
    int field_18;
    int field_1C;
    int location_table_size;
    int field_24;
    int memory_map_table_size;
    int field_2C;
    int prerequisite_table_size;
    int field_34;

    resource_amalgapak_header()
    {

    }

    void clear()
    {

    void(__fastcall * func)(void*, void*) = (decltype(func))0x0050E650;
    func(this, nullptr);
}


bool verify(const mString& a2)
{

    void(__fastcall * func)(void*, void*) = (decltype(func))0x0050E6B0;
    func(this, nullptr);

    return 0;
}
};
