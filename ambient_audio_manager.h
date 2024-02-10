#pragma once

#include "float.hpp"

struct ambient_audio_manager {
    ambient_audio_manager();

    //0x0053EC10
    static void create_inst();

    //0x00559380
    static void frame_advance(Float a1);

    //0x0054DF90
    static void reset();
};
