#pragma once

#include "float.hpp"

#include <cstdint>

struct sound_alias_database;


static constexpr int SM_MAX_SOURCE_TYPES = 8;

static Var<bool> s_sound_manager_initialized{0x0095C829};

struct sound_volume {
    float field_0;
    int field_4[7];
};


struct sound_manager {
    void create_inst()
    {
        CDECL_CALL(0x00543500);
    }

    void frame_advance(Float a1)
    {
        CDECL_CALL(0x00551C20, a1);
    }

    void set_source_type_volume(unsigned int source_type, Float a2, Float a3)
    {
        CDECL_CALL(0x0050FC50, source_type, a2, a3);
    }

    void unpause_all_sounds()
    {
        CDECL_CALL(0x00520520);
    }

    static void load_common_sound_bank(bool a1)
    {
    CDECL_CALL(0x0054DB10, a1);
}
};