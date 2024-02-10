#pragma once

#include "entity.h"

struct mic : entity {
    float field_68[3];
    float field_74[3];

    //0x0050B930
    mic(entity *a2, const string_hash &a3);

    //0x0051D9A0
    //virtual
    void frame_advance(Float a2);
};
