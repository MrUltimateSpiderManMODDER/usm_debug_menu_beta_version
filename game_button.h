#pragma once

#include "float.hpp"
#include "input_mgr.h"
#include "commands.h"

#include <cstdint>

inline constexpr auto GBFLAG_PRESSED = 1;
inline constexpr auto GBFLAG_TRIGGERED = 2;
inline constexpr auto GBFLAG_RELEASED = 4;

struct game_button {
    int m_trigger_type;
    game_control_t field_8;
    game_button *field_C;
    game_button *field_10;
    float field_14;
    float field_18;
    float field_1C;
    float field_20;
    float field_24;
    float field_28;
    float field_2C;
    int16_t field_30;
    int16_t m_flags;

    //0x0048D9A0
    game_button();

    //0x0048C6F0
    ~game_button();

    double sub_55ED50();

    double sub_55ED30();

    bool sub_48B290();

    bool sub_48B270();

    //0x0050B640
    void override(Float a2, Float a3, Float a4);

    //0x0051D510
    void update(Float a2);

    void sub_48C800(game_button *a2);

    //0x0048C770
    void set_control(game_control_t a2);

    //0x0050B610
    void clear();
};
