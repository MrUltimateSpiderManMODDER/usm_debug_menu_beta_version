#pragma once


#include "color32.h"
#include "common.h"
#include "entity.h"
#include "entity_handle_manager.h"
#include "fetext.h"
#include "fixedstring.h"
#include "float.hpp"
#include "game_button.h"
#include "input_mgr.h"
#include "limited_timer.h"
#include "memory.h"
#include "message_board.h"
#include "mstring.h"
#include "ngl.h"
#include "os_developer_options.h"
#include "physical_interface.h"
#include "po.h"
#include "region.h"
#include "rumble_struct.h"
#include "terrain.h"
#include "utility.h"
#include "variable.h"
#include "vector2di.h"
#include "wds.h"
#include "geometry_manager.h"
#include "cut_scene_player.h"
#include "ai_find_best_swing_anchor.h"
#include "daynight.h"
#include "event_manager.h"
#include "femanager.h"
#include "fetext.h"
#include "frontendmenusystem.h"
#include "aeps.h"
#include "ambient_audio_manager.h"
#include "mission_stack_manager.h"
#include "resource_key.h"








#include "mic.h"
#include "wds.h"

struct game_process {
    const char* field_0;
    int* field_4;
    int index;
    int num_states;
    int field_10;
    bool field_14;
};

inline Var<game_process> lores_game_process { 0x00922074 };

struct game_settings;
struct message_board;
struct world_dynamics_system;
struct entity_base;
struct localized_string_table;
struct game_process;
struct camera;
struct input_mgr;
struct mic;
struct nglMesh;
struct vector2di;
struct resource_key;
struct level_descriptor_t;
struct level_load_stuff;

enum class game_state {
    LEGAL = 1,
    WAIT_LINK = 4,
    LOAD_LEVEL = 5,
    RUNNING = 6,
    PAUSED = 7,
};

struct game {
    char field_0[0x5C];
    entity* field_5C;
    entity* current_game_camera;
    void* field_64;
    void* field_68;
    struct {
        int field_0;
        game_process* m_first;
        game_process* m_last;
        game_process* m_end;

        auto& back()
        {
            return *(m_last - 1);
        }
    } process_stack;

    int field_7C[17];
    game_settings* gamefile;
    int field_C4[41];

    struct flag_t {
        bool level_is_loaded;
        bool single_step;
        bool physics_enabled;
        bool field_3;
        bool game_paused;
    } flag;
    bool field_16D;
    bool field_16E;
    bool m_hero_start_enabled;
    bool field_170;
    bool field_171;
    bool m_user_camera_enabled;
    char empty5[253];
    int field_270;
    int field_274;
    float field_278;
    int field_27C;
    int field_280;
    float field_284;
    int field_288;
    float field_28C;
    bool field_2B4;
    bool field_2B5;
    int field_2B8;
    int field_2BC;
    limited_timer_base* field_2C0;
    bool field_15C;
    bool field_15D;
    bool field_15E;
    bool field_15F;
    bool field_160;
    bool field_167;
    world_dynamics_system* the_world;

    level_load_stuff* level;
    

    void enable_user_camera(bool a2)
    {
        this->m_user_camera_enabled = a2;
    }

    bool is_user_camera_enabled() const
    {
        return m_user_camera_enabled;
    }
    struct level_load_stuff {
        level_descriptor_t* descriptor;
        mString name_mission_table;
        mString field_14;
        vector3d field_24;
        int field_30;
        limited_timer_base field_34;
        bool load_widgets_created;
        bool load_completed;
        bool field_3A;
        bool field_3B;
        


        void construct_loading_widgets()
        {
            void(__fastcall * func)(void*, void*) = (decltype(func))0x0051D260;

            func(this, nullptr);
        }

        void destroy_loading_widgets()
        {
            void(__fastcall * func)(void*, void*) = (decltype(func))0x0050B560;

            func(this, nullptr);
        }

        void load_complete()
        {
            this->load_completed = true;
        }
    };

    game_settings* get_game_settings()
    {
        assert(gamefile != nullptr);

        return this->gamefile;
    }

    void sub_5BC870()
    {
        CDECL_CALL(0x005BC870);
    }

    void enable_physics(bool a2)
    {
        void(__fastcall * func)(void*, int, bool) = (decltype(func))0x00515230;
        func(this, 0, a2);
    }

    bool is_physics_enabled() const
    {
        return this->flag.physics_enabled;
    }

    void enable_marky_cam(bool a2, bool a3, Float a4, Float a5)
    {

        void(__fastcall * func)(void*, void*, bool, bool, Float, Float) = (decltype(func))0x005241E0;

        func(this, nullptr, a2, a3, a4, a5);
    }

    void push_process(game_process& process)
    {
        void(__fastcall * sub_570FD0)(void*, int, void*) = (decltype(sub_570FD0))0x00570FD0;

        sub_570FD0(&this->process_stack, 0, &process);

        auto& last_proc = this->process_stack.back();
        last_proc.index = 0;
        last_proc.field_10 = 0;
    }

    void push_lores()
    {
        this->push_process(lores_game_process());
    }




    void load_hero_packfile(const char* str, bool a3)
    {
        void(__fastcall * func)(void*, void*, const char* str, bool a3) = (decltype(func))0x0055A420;

        func(this, nullptr, str, a3);
    }





    void begin_hires_screenshot(int a2, int a3)
    {
        void(__fastcall * func)(void*, void*, int, int) = (decltype(func))0x00548C10;
        func(this, nullptr, a2, a3);
    }

    void sub_682FAB(float a1)
    {
        sub_FC4D50(a1);
    }

    void sub_66B289(nglFont* a1, float _8, float a3, float a4, const char* a2)
    {
        sub_FDC0A0(a1, _8, a3, a4, a2);
    }

    bool wait_for_mem_check()
    {
        bool result = false;

        if (g_femanager().m_fe_menu_system != nullptr) {
            result = g_femanager().m_fe_menu_system->WaitForMemCheck();
        }

        return result;
    }



 

    

      void advance_state_legal(Float a2)
        {
            void(__fastcall * func)(void*, void*, Float) = (decltype(func))0x00558100;

            func(this, nullptr, a2);

        }




void advance_state_paused(Float a1)
        {
            void(__fastcall * func)(void*, void*,Float) = (decltype(func))0x00558220;

            func(this, nullptr, a1);

        }





void init_motion_blur()
        {
            void(__fastcall * func)(void*, void*) = (decltype(func))0x00514AB0;

            func(this, nullptr);
        }




        


        void sub_579290()
        {
            ;
        }

void look_up_level_descriptor()
        {

            void(__fastcall * func)(void*, void*) = (decltype(func))0x0055A1C0;
            func(this, nullptr);
        }




    mString get_hero_info();

    mString get_camera_info();

    mString get_analyzer_info();

    void show_debug_info();

    void handle_cameras(input_mgr* a2, const Float& a3);

    void frame_advance_level(Float time_inc);

    void clear_screen();

    void advance_state_load_level(Float a2);


    void load_new_level(const mString& a1, int a2);

    void us_lighting_switch_time_of_day(int a1);

    void unload_current_level();

    void handle_frame_locking(float* a1);

    void frame_advance(Float a2);

    void load_new_level_internal(const mString& a2);

    void game__setup_inputs(game* a1);

    void game__setup_input_registrations(game* a1);

    void render_empty_list();

    void load_this_level();



     game();
};

VALIDATE_SIZE(game::level_load_stuff, 0x3C);
VALIDATE_OFFSET(game, m_user_camera_enabled, 0x172);
VALIDATE_OFFSET(game, field_280, 0x280);

inline Var<game*> g_game_ptr { 0x009682E0 };


inline Var<game*> g_is_the_packer { 0x009682E4 };

inline Var<int> g_TOD { 0x0091E000 };

mString sub_55DD80(const vector3d& a2)
{
    mString a1 { 0, "<%.3f,%.3f,%.3f>", a2.x, a2.y, a2.z };
    return a1;
}

mString game::get_camera_info()
{
    auto* v2 = this->field_5C;

    mString v22;
    if (v2->get_primary_region() != nullptr) {
        auto* v4 = v2->get_primary_region();
        auto& v5 = v4->get_name();
        auto* v6 = v5.to_string();
        v22 = mString { v6 };
    } else {
        v22 = mString { "none" };
    }

    mString v33 = v22;

    auto& v18 = *v2->get_abs_position();
    auto* v8 = g_world_ptr()->the_terrain;
    auto* v32 = v8->find_region(v18, nullptr);
    if (v32 != nullptr) {
        auto& v9 = v32->get_name();
        auto* v10 = v9.to_string();
        v33 = { v10 };
    }

    auto& v12 = *v2->get_abs_position();
    auto v31 = sub_55DD80(v12);

    auto& v14 = *v2->get_abs_po();
    auto& v15 = v14.get_z_facing();

    auto v30 = sub_55DD80(v15);
    auto* v20 = v30.c_str();
    auto* v19 = v31.c_str();
    auto* v16 = v33.c_str();

    mString v29 { 0, "CAMERA @ %s %s, f = %s", v16, v19, v20 };

    auto v24 = " " + v33;

    v29 += v24;

    return v29;
}

   Var<int*> g_debug = 1;



Var<int> g_debug_mem_dump_frame { 0x00921DCC };

      game::game()
{

    float fov = os_developer_options::instance()->get_int(mString { "CAMERA_FOV" }) * 0.017453292;

    geometry_manager::set_field_of_view(fov);

    g_debug()[0] |= 0x80u;
    if (os_developer_options::instance()->get_flag(mString { "OUTPUT_WARNING_DISABLE" })) {
        g_debug()[0] &= 0x7Fu;
    }

    g_debug()[1] |= 4u;
    if (os_developer_options::instance()->get_flag(mString { "OUTPUT_ASSERT_DISABLE" })) {
        g_debug()[1] &= 0xFBu;
    }

    g_debug_mem_dump_frame() = os_developer_options::instance()->get_int(mString { "MEM_DUMP_FRAME" });

    CDECL_CALL(0x00557610, this);
}

          
      
      



   Var<char*> g_scene_name = 1024;

void game::load_new_level_internal(const mString& a2)
{
    if (a2.m_size) {
        strcpy(g_scene_name(), a2.c_str());
    }

    this->field_15D = true;
    this->field_15C = false;
    this->field_167 = true;
}

void game::load_new_level(const mString& a1, int a2)
{
    if constexpr (1) {
        this->load_new_level_internal(a1);
        this->m_hero_start_enabled = true;

    } else {
        void(__fastcall * func)(void*, void*, const mString*, int) = (decltype(func))0x00514C70;

        func(this, nullptr, &a1, a2);
    }
}


    Var<int> g_mem_checkpoint_level = 0x00921DC4;




void game::unload_current_level()
{
    if constexpr (0) {
        mem_print_stats("unload_current_level() start");

        this->field_170 = true;

        daynight::lights() = nullptr;
        daynight::gradients() = nullptr;
        daynight::initialized() = false;

        this->flag.level_is_loaded = false;

        assert(the_world != nullptr);

        event_manager::clear();

        g_world_ptr() = nullptr;

        swing_anchor_finder::remove_all_anchors();
        this->flag.level_is_loaded = 0;

        mem_print_stats("unloading level");

        auto* mem = operator new(0x400u);

        auto* v16 = new (mem) world_dynamics_system {};

        g_world_ptr() = v16;
        sub_579290();
        this->field_170 = false;

        mem_print_stats("unload level end");

        void(__fastcall * func)(void*, void*) = (decltype(func))0x00557E10;
        func(this, nullptr);
    }
}

mString game::get_analyzer_info()
{
    auto v16 = string_hash("SCENE_ANALYZER_CAM");
    auto* v3 = entity_handle_manager::find_entity(v16, entity_flavor_t::CAMERA, false);

    auto& v14 = *v3->get_abs_position();
    auto* v4 = g_world_ptr()->the_terrain;
    auto* v26 = v4->find_region(v14, nullptr);

    mString v25 { "" };
    if (v26 != nullptr) {
        auto& v5 = v26->get_name();
        auto* v6 = v5.to_string();
        v25 = v6;
    }

    auto& v8 = *v3->get_abs_position();
    auto v24 = sub_55DD80(v8);

    auto& v10 = *v3->get_abs_po();
    auto& v11 = v10.get_z_facing();
    auto v23 = sub_55DD80(v11);

    auto* v15 = v23.c_str();
    auto* v12 = v24.c_str();

    mString a1 { 0, "ANALYZER @ %s, f = %s", v12, v15 };
    auto v17 = " " + v25;
    a1 += v17;
    return a1;
}

mString game::get_hero_info()
{
    auto* hero_ptr = g_world_ptr()->get_hero_ptr(0);
    if (hero_ptr == nullptr) {
        mString result { "(hero does not exist!)" };
        return result;
    }

    region* v29 = nullptr;
    if (hero_ptr != nullptr) {
        v29 = hero_ptr->get_primary_region();
    }

    mString v28 { "none" };
    if (v29 != nullptr) {
        auto& v4 = v29->get_name();
        auto* v5 = v4.to_string();
        v28 = { v5 };
    }

    auto v27 = [&]() -> mString {
        if (hero_ptr != nullptr) {
            auto& v6 = *hero_ptr->get_abs_position();
            return sub_55DD80(v6);
        }

        return mString { "N/A" };
    }();

    vector3d v15;
    if (hero_ptr != nullptr) {
        auto* v7 = bit_cast<actor*>(hero_ptr)->physical_ifc();
        v15 = v7->get_velocity();
    } else {
        v15 = ZEROVEC;
    }

    mString v25 { 0, "HERO @ %s ", v28.c_str() };

    auto* v9 = v27.c_str();
    v25.append(v9, -1);
    v25.append(", v = ", -1);

    auto v14 = sub_55DD80(v15);
    v25.append(v14.c_str(), -1);

    return v25;
}

void game::show_debug_info()
{
    auto DEBUG_INFO_FONT_PCT = os_developer_options::instance()->get_int(mString { "DEBUG_INFO_FONT_PCT" });
    auto v15 = (float)DEBUG_INFO_FONT_PCT / 100.0;
    auto a1 = this->get_hero_info();

    vector2di v13 { 50, 40 };
    auto* v4 = a1.c_str();
    nglListAddString(nglSysFont(), (float)v13.x, (float)v13.y, 1.0, v15, v15, v4);

    auto v12 = this->get_camera_info();
    v13.y += 20;
    auto* v5 = v12.c_str();
    nglListAddString(nglSysFont(), (float)v13.x, (float)v13.y, 1.0, v15, v15, v5);

    auto v11 = this->get_analyzer_info();
    v13.y += 20;
    auto* v6 = v11.c_str();
    nglListAddString(nglSysFont(), (float)v13.x, (float)v13.y, 1.0, v15, v15, v6);
}

void game::handle_cameras(input_mgr* a2, const Float& a3)
{
    printf("game::handle_cameras");

    printf("%d %d", this->is_user_camera_enabled(), os_developer_options::instance()->get_int(0));

    if constexpr (0) {
    } else {
        void(__fastcall * func)(void*, void* ,input_mgr*, const Float&) = (decltype(func))0x00552F50;

        func(this, nullptr, a2, a3);
    }
}

void game::load_this_level()
{

        void(__fastcall * func)(void*, void*) = (decltype(func))0x0055C6E0;
        func(this, nullptr);
    }





void game::advance_state_load_level(Float a2)
{

    static Var<bool> loading_a_level { 0x00960CB5 };
    if (!loading_a_level()) {
    loading_a_level() = true;
    auto& v9 = this->level;
    if (!g_is_the_packer()) {
            sound_manager::load_common_sound_bank(true);
    }
   
        mission_stack_manager::s_inst()->start_streaming();


    this->load_this_level();
}

    
            int v10 = os_developer_options::instance()->get_int(mString { "TIME_OF_DAY" });
if (v10 == -1) {
    v10 = g_TOD();
}
    this->flag.level_is_loaded = true;
    this->field_167 = false;
    loading_a_level() = false;


        cut_scene_player* v13 = g_cut_scene_player();
            v13 = g_cut_scene_player();
            v13->frame_advance(a2);
    void(__fastcall * func)(void*, void* edx, Float) = (decltype(func))0x0055D3A0;
    func(this, nullptr, a2);
    }
    
    



void game::frame_advance_level(Float time_inc)
{
    printf("game::frame_advance_level\n");

    {
        static int dword_14EEAC4 { -1 };
        mem_check_leaks_since_checkpoint(dword_14EEAC4, 1);
        dword_14EEAC4 = mem_set_checkpoint();
        mem_print_stats("\nMemory log\n");
    }

    void(__fastcall * func)(void*, void* edx, Float) = (decltype(func))0x0055D650;
    func(this, nullptr, time_inc);
}

void game::clear_screen()
{


        void(__fastcall * func)(void*, void* edx) = (decltype(func))0x00515140;
        func(this, nullptr);
    }


void game::handle_frame_locking(float* a1)
{
    auto frame_lock = os_developer_options::instance()->get_int(mString { "FRAME_LOCK" });
    if (frame_lock > 0) {
        *a1 = 1.0 / frame_lock;
    }
}
    

void render_text(const mString& a1, const vector2di& a2, color32 a3, float a4, float a5)
{
    if (os_developer_options::instance()->get_flag(mString { "SHOW_DEBUG_TEXT" })) {
        FEText fe_text { font_index { 0 },
            global_text_enum { 0 },
            (float)a2.x,
            (float)a2.y,
            (int)a4,
            panel_layer { 0 },
            a5,
            16,
            0,
            a3 };

        fe_text.field_1C = a1;

        fe_text.Draw();
    }
}



void game::game__setup_inputs(game* a1)
{
    void(__fastcall * func)(void*, void* edx, game*) = (decltype(func))0x00605950;
    func(this, nullptr, a1);
}

void game::game__setup_input_registrations(game* a1)
{
    void(__fastcall * func)(void*, void* edx, game*) = (decltype(func))0x006063C0;
    func(this, nullptr, a1);
}

void game::render_empty_list()
{

    void(__fastcall * func)(void*, void*) = (decltype(func))0x00510780;
    func(this, nullptr);
}

void game::frame_advance(Float a2)
{

    void(__fastcall * func)(void*, void* edx, Float) = (decltype(func))0x0055D780;
    func(this, nullptr, a2);
}



void game::us_lighting_switch_time_of_day(int a1)
{
    void(__fastcall * func)(void*, void* edx, int) = (decltype(func))0x00408790;
    func(this, nullptr, a1);

}

void system_idle()
{
    MSG Msg;

    while (PeekMessageA(&Msg, nullptr, 0, 0, 1u)) {
        if (Msg.message == WM_QUIT) {
            break;
        }

        if (Msg.message == WM_CLOSE) {
            break;
        }

        if (Msg.message == WM_DESTROY) {
            break;
        }

        TranslateMessage(&Msg);
        DispatchMessageA(&Msg);
    }
}





void register_chuck_callbacks()
{
    printf("register_chuck_callbacks");

    CDECL_CALL(0x006607E0);
}

void game_patch()
{
    {
        auto address = func_address(&game::frame_advance_level);
        REDIRECT(0x0055D8C2, address);

    }

    {
        auto address = func_address(&game::load_hero_packfile);
        REDIRECT(0x0055B44E, address);
    
        }

    {
//    auto address = func_address(&game::advance_state_load_level);
//   REDIRECT(0x0055D55D, address);
}

    {
        auto address = func_address(&game::frame_advance);
        REDIRECT(0x005D70B8, address);
    
    }

            {
        REDIRECT(0x0052B5A6, render_text);
        REDIRECT(0x00514DA8, render_text);
        REDIRECT(0x00514E4B, render_text);
    }
            {
        auto address = func_address(&game::handle_cameras);
        REDIRECT(0x0055D74F, address);
            }
            {
        auto address = func_address(&game::advance_state_legal);
        REDIRECT(0x0055D539, address);
            }
            {
        auto address = func_address(&game::clear_screen);
            REDIRECT(0x00557BC3, address);
            REDIRECT(0x00557BF9, address);
}
}

void wds_patch()
{
{
            if constexpr (1) {
            auto address = func_address(&world_dynamics_system::add_player);
            REDIRECT(0x0055CCA3, address);
            REDIRECT(0x005C5889, address);
            REDIRECT(0x00641C5C, address);
            REDIRECT(0x00641C97, address);
            REDIRECT(0x00676CFB, address);
            }

            return;

}
}

void level_patch()
{
    {
        auto address = func_address(&game::level_load_stuff::construct_loading_widgets);
        REDIRECT(0x0055D402, address);
    }

    {
        auto address = func_address(&game::level_load_stuff::destroy_loading_widgets);
        REDIRECT(0x0055D43C, address);
    }
}

void mission_manager_patch()
{
    {
        auto address = func_address(&mission_manager::load_script);
        REDIRECT(0x005E1B1F, address);
    }

    {
        auto address = func_address(&mission_manager::run_script);
        REDIRECT(0x005E1A96, address);
        REDIRECT(0x005E1B2B, address);
    }

    {
        /*auto address = func_address(&mission_manager::show_mission_loading_panel);
        REDIRECT(0x005DEF4C, address);
        */
    }

    {
        auto address = func_address(&mission_manager::unload_script_if_requested);
        REDIRECT(0x005DBE99, address);
        REDIRECT(0x005E1A65, address);
    }
    {
        auto address = func_address(&mission_manager::frame_advance);
        REDIRECT(0x0055D75B, address);
    }

    {
        /* auto address = func_address(&mission_manager::kill_braindead_script);
        SET_JUMP(0x005D7EF0, address);

    */
    }
}


