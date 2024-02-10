#pragma once

#include "variable.h"

#include "float.hpp"

struct city_lights;
struct city_gradients;

struct daynight {
    //0x00527F80
    static void frame_advance(Float a1);

    //0x0051BCA0
    static void update_shadow_settings();

    //0x00550690
    static void init();

    //0x0051BC50
    static void kill();

    static inline Var<float> current_time = {0x0095C1EC};

    static inline Var<float> glow_intensity = {0x0095C71C};

    static inline Var<float> shadow_multiplier = {0x0095C168};

    static inline Var<city_lights *> lights = {0x0095C954};

    static inline Var<city_gradients *> gradients{0x0095C950};

    static inline Var<bool> initialized{0x0095C94C};
};

extern void daynight_patch();
