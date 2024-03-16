#pragma once

#include "variable.h"
#include "entity_base.h"
#include "bit.h"


struct entity;
struct terrain;

struct world_dynamics_system
{
    
    char field_0[0x1AC];
    terrain *the_terrain;
    char field_1B0[0x40 * 2];
    entity *field_230[1];
    int field_234;
    int num_players;

    auto *get_the_terrain() {
        return the_terrain;
    }

    entity *get_hero_ptr(int index) {
        constexpr auto MAX_GAME_PLAYERS = 1;

        assert(index >= 0 && index <= MAX_GAME_PLAYERS);

        auto *result = this->field_230[index];
        return result;
    }


            
    #include <stdio.h>
#include <string.h>

    int load_hero(const char* second_str, const char* first_str)
    {
        // declaring two same string
        const char* _second_str = "True";
        const char* _first_str = "False";

        // printing the strings
        printf("second String: %s\n", second_str);

        // printing the return value of the strcmp()
        printf("Return value of strcmp(): %d",
            load_hero(second_str, first_str));

        int(__fastcall * func)(void*, void*, const char*, const char*) = (decltype(func))0x0055B400;

        return func(this, nullptr, second_str, first_str);

        return 0;
    }

 




    inline int get_num_players()
    {
        return this->num_players;
    }


        void unload_hero_packfile()
    {
        void(__fastcall * func)(void*, void*) = (decltype(func))0x00558320;

        func(this, nullptr);
    }


        Var<world_dynamics_system*> g_spiderman_camera_ptr { 0x00959A70 };


    void remove_player(int player_num)
    {

        if (this->num_players == 1) {

            

        void(__fastcall * func)(void*, void*, int) = (decltype(func))0x00558550;

        func(this, nullptr, player_num);
        }
    }


        void wds_remove_player(int player_num, debug_menu_entry* entry, bool a2)
    {

        if (this->num_players == 1) {
        entry->entry_type = BOOLEAN_E;
        entry->data = (void*)a2;

        void(__fastcall * func)(void*, void*, int) = (decltype(func))0x00558550;

        func(this, nullptr, player_num);
        }
    }


    void malor_point(const vector3d *a2, int a3, bool a4)
    {
        void(__fastcall * func)(void*, void*, const vector3d*, int, bool) = (decltype(func))0x00530460;
        func(this, nullptr, a2, a3, a4);
    }

    int add_player(const mString &a2)
    {
        int (__fastcall *func)(void *, void *, const mString *) = (decltype(func))0x0055B400;

        return func(this, nullptr, &a2);
    }


};

VALIDATE_OFFSET(world_dynamics_system, field_230, 0x230);

inline Var<world_dynamics_system *> g_world_ptr{0x0095C770};

