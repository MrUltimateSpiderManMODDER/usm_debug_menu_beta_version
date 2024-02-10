#pragma once

#include "resource_key.h"
#include "sound_instance_id.h"
#include "vector3d.h"

struct fx_cache;
struct vector3d;
struct handheld_item;
struct entity_base;

struct cached_special_effect {
    resource_key field_0;
    resource_key field_8;
    vector3d field_10;
    int field_1C;
    int field_20;
    sound_instance_id field_24;
    int field_28;
    float field_2C;
    fx_cache *field_30;
    int16_t field_34;
    int16_t field_36;
    int field_38;
    char field_3C;
    char field_3D;

    //0x005020D0
    cached_special_effect();

    //0x004EFC00
    void spawn(bool a1,
               const vector3d &a2,
               const vector3d &a3,
               handheld_item *a6,
               entity_base *a7,
               entity_base *a8,
               const vector3d &a9,
               bool a10,
               bool a11);

    //0x004D4E10
    void fill_cache();

    //0x004D81F0
    void frame_advance(Float a2);
};
