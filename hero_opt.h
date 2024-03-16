#pragma once

#include "hero_options.h"

#include <cassert>

struct hero_option_t {
    const char* m_hero_name;
    union {
        bool* p_hero_bval;
        int* p_hero_ival;
    } m_hero_value;
    enum {
        UNDEFINED = 0,
        SET_PLAYER_VALUE = 1,
    } m_hero_type;
};

constexpr auto NUM_HEROES = 18u;


const char* hero_list[18] = { "ultimate_spiderman", "arachno_man_costume", "usm_wrestling_costume", "usm_blacksuit_costume", "peter_parker", "peter_hooded", "peter_parker_costume", "peter_hooded_costume", "venom", "venom_spider", "carnage", "wolverine", "rhino", "green_goblin", "electro_suit", "silver_sable", "electro_nosuit", "mary_jane" };


hero_option_t* get_hero_opt(int idx)
{
    assert(idx >= 0);
    assert(idx < NUM_HEROES);

    static hero_option_t hero {};

    if (idx < 18) {

        auto& name = hero_list[idx];
        bool* hero_status = &hero_defaults[idx];
        bool* hero_status2 = &hero_options::instance()->m_heros[idx];

        hero.m_hero_name = name;
        hero.m_hero_type = hero_option_t::SET_PLAYER_VALUE;
        hero.m_hero_value.p_hero_bval = hero_status;


        return &hero;
    }

    idx = idx - 18;
    auto& name = hero_list[idx];
    int* i = &hero_options::instance()->m_heroes[idx];

    hero.m_hero_name = name;
    hero.m_hero_type = hero_option_t::UNDEFINED;
    hero.m_hero_value.p_hero_ival = i;

    return &hero;
}