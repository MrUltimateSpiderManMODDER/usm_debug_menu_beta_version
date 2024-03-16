#pragma once

#include "variable.h"
#include "func_wrapper.h"
#include "mstring.h"
#include "hero_opt.h"

#include <optional>

struct hero_options
{
    enum strings_t {
        SOUND_LIST = 0,
        SCENE_NAME = 1,
        HERO_NAME = 2,
        GAME_TITLE = 3,
        GAME_LONG_TITLE = 4,
        SAVE_GAME_DESC = 5,
        VIDEO_MODE = 6,
        GFX_DEVICE = 7,
        FORCE_DEBUG_MISSION = 8,
        FORCE_LANGUAGE = 9,
        SKU = 10,
        CONSOLE_EXEC = 11,
        HERO_START_DISTRICT = 12,
        DEBUG_ENTITY_NAME = 13,
    };

    bool m_heros[11];
    int m_heroes[11];
    mString m_strings[14];


    
    void set_hero(const mString& a2, bool a3)
    {
        this->m_heros[this->get_hero_from_name(a2)] = a3;
    }

const char* hero_list[18] = { "ultimate_spiderman", "arachno_man_costume", "usm_wrestling_costume", "usm_blacksuit_costume", "peter_parker", "peter_hooded", "peter_parker_costume", "peter_hooded_costume", "venom", "venom_spider", "carnage", "wolverine", "rhino", "green_goblin", "electro_suit", "silver_sable", "electro_nosuit", "mary_jane" };


    bool hero_defaults[18] = { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };


    void _tcf_1_3()
    {

    }


    int get_hero_name2(int a1)
    {
        int _tmp_4_27423 = 0;
        int result_27422 = 1; 
        if (!_tmp_4_27423) {
            mString(result_27422, "HERO");

            _tmp_4_27423 = 1;
            CDECL_CALL(0x005C3150, this);
            
        }

        return result_27422;
    }


    mString* get_hero_name(int a1) {
        if constexpr (1) {
            static Var<int> dword_96A4AC{ 0x0096A4AC };

            static Var<mString> result{ 0x0096A49C };

            void (*sub_868B40)(void) = 0(sub_868B40, 0x00868B40);

            if (!(dword_96A4AC() & 19)) {
                dword_96A4AC() |= 19u;

                result() = mString{ "hero" };
            }

            result() = this->m_strings[2];
            return (&result());
        }
        else {
            return (mString*)CDECL_CALL(0x005C3150, this);
        }
    }

    int get_hero_from_name(const mString& a1)
    {
        const char** v2 = hero_list;
        do {
            if (_strcmpi(*v2, a1.guts) == 0) {
                break;
            }

            ++v2;
        } while (v2 != (const char**)hero_defaults);
      

        int v3 = v2 - hero_list;
        if (v3 == 15) {
            printf("Nonexistent option hero %s", a1.c_str());
        }

        return v3;
    }

    bool get_hero(int a2)
    {
        return this->m_heros[a2];
    }

    bool get_hero(const mString &a2)
    {
        return this->m_heros[this->get_hero_from_name(a2)];
    }

    static inline Var<hero_options *> instance{0x0096858C};
    static inline Var<hero_options*> cheat_heroes { 0x00922564 };

};

bool hero_defaults[18] = { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };