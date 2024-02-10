#pragma once

#include "float.hpp"
#include "string_hash.h"

struct patrol_manager {
    struct patrol_sequence {
        int field_0;
        void *field_4;
        int field_8;
        int field_C;
        int field_10;
        int field_14;
        int field_18;
        int field_1C;
        int field_20;
        char field_24;
        char field_25;
        char field_26;
        char field_27;
        int field_28;
    };

    char field_0;
    patrol_sequence field_4;
    int field_30;
    string_hash field_34;

    //0x005DD280
    patrol_manager();

    //0x005DD330
    void frame_advance(Float a2);
};
