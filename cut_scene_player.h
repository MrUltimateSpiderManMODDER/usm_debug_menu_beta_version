#pragma once

#include "float.hpp"



struct cut_scene;
struct nalStreamInstance;
struct cut_scene_segment;

struct cut_scene_player {
    cut_scene *current_cut_scene;
    int field_8;
    int field_C;
    char field_10;
    int field_14;
    int field_18;
    int field_1C;
    int field_20;
    int field_24;
    int field_28;
    int field_2C;
    int field_30;
    int field_34;
    int field_38;
    int field_3C;
    int field_40;
    int field_44;
    std::vector<nalStreamInstance *> field_48;
    std::vector<nalStreamInstance *> field_58;
    int field_68[5];
    void *field_7C;
    int field_80;
    int field_84;

    int field_88[12];

    int field_B8;
    int field_BC;
    int field_C0;
    int field_C4;
    int field_C8;
    int field_CC;
    int field_D0;
    char field_D4;
    char field_D5;
    char field_D6;
    char field_D7;
    char field_D8;
    char field_D9;
    int field_DC;
    bool field_E0;
    bool field_E1;
    bool field_E2;
    int field_14C;
    int field_150;
    float field_154;
    float field_158;
    int field_15C;
    int field_160;
    int field_164;

    //0x0073F170
    cut_scene_player();

    //0x00737F60
    void advance_lip_syncing(Float a2);

    //0x007382E0
    bool advance_panel_anims(Float a2);

    //0x0073FFB0
    void clean_up_finished_segment();

    //0x00742190
    void play(cut_scene *a2);

    //0x007414E0
    void play_current_segment();

    bool sub_741220(Float a2);


    void frame_advance(Float a2)
    {
        // sp_log("cut_scene_player::frame_advance");


         void(__fastcall * func)(void*, void*, Float) = (decltype(func))0x00741EC0;

        func(this, nullptr, a2);
    }

    //0x00502B00
    bool is_playing();

    //0x00740660
    void stop(cut_scene *a2);

    //0x0086DE50
    static void finalize();
};


static Var<cut_scene_player> player { 0x0096FEB8 };

cut_scene_player* g_cut_scene_player()
{
    if constexpr (0) {
        static Var<uint32_t> dword_970020 { 0x00970020 };
        if (!(dword_970020() & 1)) {
            dword_970020() |= 1u;
            player() = cut_scene_player {};
            atexit(&cut_scene_player::finalize);
        }

        return &player();
    } else {
        return (cut_scene_player*)CDECL_CALL(0x007411C0);

    }
}

extern void cut_scene_player_patch();
