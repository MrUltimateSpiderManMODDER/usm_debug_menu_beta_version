#include "actor.h"
#include "ai_player_controller.h"
#include "common.h"
#include "debug_menu.h"
#include "entity.h"
#include "fe_health_widget.h"
#include "femanager.h"
#include "fixedstring.h"
#include "game.h"
#include "igofrontend.h"
#include "mstring.h"
#include "resource_key.h"
#include "resource_manager.h"
#include "resource_partition.h"
#include "string_hash.h"
#include "wds.h"


#include <cassert>

enum class hero_status_e {
    UNDEFINED = 0,
    REMOVE_PLAYER = 1,
    ADD_PLAYER = 2,
    ADD_PLAYER_ = 3,
    CHANGE_HEALTH_TYPE = 4,
} hero_status;

enum class hero_enabled_e {
     SET_TRUE = 1,
} hero_enabled;

enum class hero_disabled_e {
    SET_FALSE = 0,
} hero_disabled;




constexpr auto NUM_CHARS = 38;

const char* char_list[] = {
    "ch_venom_viewer",
    "ch_vwr_sable",
    "ch_vwr_johnny_storm",
    "ch_vwr_gmu_cop_fat",
    "ch_vwr_gmu_sable_merc",
    "ch_vwr_shield_agent",
    "ch_vwr_gang_mercs_base",
    "ch_vwr_gang_mercs_boss",
    "ch_vwr_gang_mercs_lt",
    "ch_vwr_gang_ftb_base",
    "ch_vwr_gang_ftb_boss",
    "ch_vwr_gang_ftb_lt",
    "ch_gang_skin_base",
    "ch_vwr_gang_skin_boss",
    "ch_vwr_gang_skin_lt",
    "ch_vwr_gang_srk_base",
    "ch_vwr_gang_srk_boss",
    "ch_vwr_gang_srk_lt",
    "ch_vwr_electro_suit",
    "ch_vwr_electro_nosuit",
    "gmu_businessman",
    "gang_hellions",
    "ch_vwr_shocker",
    "ch_vwr_boomerang",
    "beetle_viewer",
    "ch_venom_eddie",
    "ch_venom_spider",
    "ultimate_spiderman_vwr",
    "ch_usm_blacksuit",
    "ch_vwr_green_goblin",
    "ch_vwr_carnage",
    "ch_mary_jane",
    "ch_vwr_rhino",
    "ch_gmu_child_male",
    "ch_gmu_cop_thin",
    "ch_gmu_man",
    "ch_gmu_woman",
    "gang1"
};





int hero_selected;
int frames_to_skip = 2;

struct level_descriptor_t {
    fixedstring<32> field_0;
    fixedstring<64> field_20;
    fixedstring<16> field_60;
    int field_70;
    int field_74;
    int field_78;
    int field_7C;
    int field_80;
    int field_84;
    int field_88;
    int field_8C;
};

VALIDATE_SIZE(level_descriptor_t, 0x90);

struct hero_descriptor_t {
    fixedstring<32> field_0;
    fixedstring<64> field_20;
    fixedstring<16> field_60;
    int field_70;
    int field_74;
    int field_78;
    int field_7C;
    int field_80;
    int field_84;
    int field_88;
    int field_8C;
};



VALIDATE_SIZE(hero_descriptor_t, 0x90);

void reboot_handler(debug_menu_entry* a1)
{
}

void handle_hero_select_menu(debug_menu_entry* entry, custom_key_type)
{
    entry->m_game_flags_handler(entry);
}

void hero_entry_callback(debug_menu_entry* entry);

void hero_toggle_handler(debug_menu_entry* entry);

void populate_level_select_menu(debug_menu_entry* entry);

const char *current_costume = "ultimate_spiderman";

const char* hero_list[18] = { "ultimate_spiderman", "arachno_man_costume", "usm_wrestling_costume", "usm_blacksuit_costume", "peter_parker", "peter_hooded", "peter_parker_costume", "peter_hooded_costume", "venom", "venom_spider", "carnage", "wolverine", "rhino", "green_goblin", "electro_suit", "silver_sable", "electro_nosuit", "mary_jane" };

void handle_hero_select_entry(debug_menu_entry* entry);

void handle_hero_select_entry(debug_menu_entry* entry)
{

    for (auto num_players = g_world_ptr()->num_players;
         num_players != 0;
         num_players = g_world_ptr()->num_players) {
        // printf("some_number %d %d\n", *some_number, num_players);
        g_world_ptr()->remove_player(num_players - 1);
    }

    debug_enabled = 0;
    frames_to_skip = 2;
    current_costume = entry->text;
    BYTE* val = (BYTE*)entry->data;
    *val = !*val;
    close_debug();
}

typedef int(__fastcall* game_pause_unpause_ptr)(void*);
game_pause_unpause_ptr game_paused = (game_pause_unpause_ptr)0x0054FBE0;
game_pause_unpause_ptr game_unpaused = (game_pause_unpause_ptr)0x0053A880;

void start_hero()
{
    debug_enabled = 0;
    menu_disabled = 0;
    game_unpaused(g_game_ptr());
    g_game_ptr()->enable_physics(true);
}



void character_toggle_handler(debug_menu_entry* entry)
{
    printf("hero_toggle_handler\n");
    assert(entry->get_id() < 38);
    hero_selected = entry->get_id();
    hero_status = hero_status_e::REMOVE_PLAYER;
}

void character_entry_callback(debug_menu_entry* entry)
{
    printf("hero_entry_callback: character_status = %d\n", hero_status);

    auto v18 = g_world_ptr()->num_players;
    switch (hero_status) {
    case hero_status_e::REMOVE_PLAYER: {
        auto v3 = entry->get_bval();
        g_world_ptr()->remove_player(v18 - 1);
        hero_status = hero_status_e::ADD_PLAYER;
        frames_to_skip = 2;
        g_game_ptr()->enable_marky_cam(true, true, -1000.0, 0.0);
        break;
    }
    case hero_status_e::ADD_PLAYER: {
        auto v1 = frames_to_skip--;
        if (v1 <= 0) {
            assert(hero_selected > -1 && hero_selected < NUM_CHARS);

            [[maybe_unused]] auto v2 = g_world_ptr()->add_player(mString { char_list[hero_selected] });

            /*
            auto v10 = v2 <= v18;

            assert(v10 && "Cannot add another_player");
            */

            g_game_ptr()->enable_marky_cam(false, true, -1000.0, 0.0);
            frames_to_skip = 2;
            hero_status = hero_status_e::CHANGE_HEALTH_TYPE;
        }
        break;
    }
    case hero_status_e::CHANGE_HEALTH_TYPE: {
        auto v3 = frames_to_skip--;
        if (v3 <= 0) {
            auto v17 = 0;
            auto* v5 = (actor*)g_world_ptr()->get_hero_ptr(0);
            auto* v6 = v5->get_player_controller();
            auto v9 = v6->field_420;
            switch (v9) {
            case 1:
                v17 = 0;
                break;
            case 2:
                v17 = 4;
                break;
            case 3:
                v17 = 5;
                break;
            }

            auto* v7 = g_world_ptr()->get_hero_ptr(0);
            auto v8 = v7->my_handle;
            g_femanager().IGO->hero_health->SetType(v17, v8.field_0);
            g_femanager().IGO->hero_health->SetShown(false);
            close_debug();
            hero_status = hero_status_e::UNDEFINED;
            start_hero();
        }
        break;
    }
    default:
        break;
    }
}





level_descriptor_t* get_level_descriptors(int* arg0)
{
    auto* game_partition = resource_manager::get_partition_pointer(0);
    assert(game_partition != nullptr);

    assert(game_partition->get_pack_slots().size() == 1);

    auto& v2 = game_partition->get_pack_slots();
    auto* game_slot = v2.front();
    assert(game_slot != nullptr);

    auto v6 = 9;
    string_hash v5 { "level" };
    resource_key a1 { v5, v6 };
    int a2 = 0;
    auto* v11 = (level_descriptor_t*)game_slot->get_resource(a1, &a2, nullptr);

    if (arg0 != nullptr) {
        *arg0 = a2 / sizeof(level_descriptor_t);
    }

    return v11;
}

hero_descriptor_t* get_hero_descriptors(int* arg0)
{
    auto* game_partition = resource_manager::get_partition_pointer(0);
    assert(game_partition != nullptr);

    assert(game_partition->get_pack_slots().size() == 1);

    auto& v2 = game_partition->get_pack_slots();
    auto* game_slot = v2.front();
    assert(game_slot != nullptr);

    auto v6 = 9;
    string_hash v5 { "hero" };
    resource_key a1 { v5, v6 };
    int a2 = 0;
    auto* v11 = (hero_descriptor_t*)game_slot->get_resource(a1, &a2, nullptr);

    if (arg0 != nullptr) {
        *arg0 = a2 / sizeof(hero_descriptor_t);
    }

    return v11;
}

void level_select_handler(debug_menu_entry* entry)
{
    auto* v1 = entry->text;
    mString v15 { v1 };

    int arg0;
    auto* v13 = get_level_descriptors(&arg0);
    for (auto i = 0; i < arg0; ++i) {
        auto* v2 = v15.c_str();
        fixedstring<16> v6 { v2 };
        if (v13[i].field_60 == v6) {
            auto* v3 = v13[i].field_0.to_string();
            v15 = { v3 };
            break;
        }
    }

    g_game_ptr()->load_new_level(v15,-1);
}






void populate_level_select_menu(debug_menu_entry* entry)
{
    // assert(debug_menu::root_menu != nullptr);



    auto* head_menu = create_menu("Level Select", debug_menu::sort_mode_t::ascending);
    entry->set_submenu(head_menu);


    auto* hero_select_menu = create_menu("Hero Select", debug_menu::sort_mode_t::ascending);
    debug_menu_entry v28 { hero_select_menu };

    head_menu->add_entry(&v28);
    for (auto i = 0u; i < 18u; ++i) {
        auto v6 = RESOURCE_KEY_TYPE_PACK;
        string_hash v5 { (hero_list)[i] };
        auto v11 = resource_key { v5, v6 };
        auto v30 = resource_manager::get_pack_file_stats(v11, nullptr, nullptr, nullptr);
        if (v30) {
            mString v35 { hero_list[i] };
            debug_menu_entry v37 { v35.c_str() };

            v37.set_game_flags_handler(hero_toggle_handler);
            v37.set_bval2(false);
            v37.m_id = i;
            v37.set_frame_advance_cb(hero_entry_callback);
            hero_select_menu->add_entry(&v37);
        }
            }
        
    
 
   

        mString v26 { "city" };
        debug_menu_entry v39 { v26.c_str() };

        v39.set_game_flags_handler(level_select_handler);

        head_menu->add_entry(&v39);




    

    mString v25 { "-- REBOOT --" };
    debug_menu_entry v40 { v25.c_str() };

    v40.set_game_flags_handler(reboot_handler);

    head_menu->add_entry(&v40);



   
}

    



void create_level_select_menu(debug_menu* parent)
{
    auto* level_select_menu = create_menu("Level Select", debug_menu::sort_mode_t::undefined);
    auto* v2 = create_menu_entry(level_select_menu);
    v2->set_game_flags_handler(populate_level_select_menu);
    parent->add_entry(v2);
}


struct debug_menu_entry;





void hero_toggle_handler(debug_menu_entry* entry)
{
    printf("hero_toggle_handler\n");
    assert(entry->get_id() < 11);
    hero_selected = entry->get_id();
    hero_status = hero_status_e::REMOVE_PLAYER;
}



void hero_entry_callback(debug_menu_entry* entry)
{
    printf("hero_entry_callback: hero_status = %d\n", hero_status);

    auto v18 = g_world_ptr()->num_players;
    switch (hero_status) {
    case hero_status_e::REMOVE_PLAYER: {
        g_world_ptr()->remove_player(v18 - 1);
        hero_status = hero_status_e::ADD_PLAYER;
        frames_to_skip = 3;
        g_game_ptr()->enable_marky_cam(true, true, -1000.0, 0.0);
        break;
    }
    case hero_status_e::ADD_PLAYER_: {
        auto v1 = frames_to_skip--;
        if (v1 <= 0) {
            assert(hero_selected > -1 && hero_selected < 11);

            [[maybe_unused]] auto v2 = g_world_ptr()->add_player(mString { hero_list[hero_selected] });

            auto v10 = v2 <= v18;

            assert(v10 && "Cannot add another_player");

            g_game_ptr()->enable_marky_cam(false, true, -1000.0, 0.0);
            frames_to_skip = 2;
            hero_status = hero_status_e::CHANGE_HEALTH_TYPE;
        }
        break;
    }
    case hero_status_e::ADD_PLAYER: {
        auto v1 = frames_to_skip--;
        if (v1 <= 0) {
            assert(hero_selected > -1 && hero_selected < 11);

            [[maybe_unused]] auto v2 = g_world_ptr()->add_player(mString { hero_list[hero_selected] });

            auto v10 = v2 <= v18;

            assert(v10 && "Cannot add another_player");

            
            g_world_ptr()->remove_player(v18 - 0);
            g_game_ptr()->enable_marky_cam(false, true, -1000.0, 0.0);
            frames_to_skip = 3;
            hero_status = hero_status_e::ADD_PLAYER_;
        }
        break;
    }
    case hero_status_e::CHANGE_HEALTH_TYPE: {
        auto v3 = frames_to_skip--;
        if (v3 <= 0) {
            auto v17 = 0;
            auto* v5 = (actor*)g_world_ptr()->get_hero_ptr(0);
            auto* v6 = v5->get_player_controller();
            auto v9 = v6->field_420;
            switch (v9) {
            case 1:
                v17 = 0;
                break;
            case 2:
                v17 = 4;
                break;
            case 3:
                v17 = 5;
                break;
            }

            auto* v7 = g_world_ptr()->get_hero_ptr(0);
            auto* v4 = g_femanager().IGO->hero_health;
            v4->field_30 = g_world_ptr()->get_hero_ptr(0)->my_handle.field_0;
            v4->field_38 = v3;
            v4->UpdateMasking();
            v4->clear_bars();
            auto v8 = v7->my_handle;
            g_femanager().IGO->hero_health->SetType(v17, v8.field_0);
            g_femanager().IGO->hero_health->SetShown(true);
            hero_status = hero_status_e::UNDEFINED;
                start_hero();


 
                
        }
        switch (hero_enabled) {
    case hero_enabled_e::SET_TRUE: {
        auto v1 = frames_to_skip--;
        if (v1 <= 0) {
            assert(hero_selected > -1 && hero_selected < 11);

            [[maybe_unused]] auto v2 = g_world_ptr()->add_player(mString { hero_list[hero_selected] });

            auto v10 = v2 <= v18;

            assert(v10 && "Cannot add another_player");

            auto v3 = entry->get_bval();
            entry->set_bval(!v3, true);
            g_world_ptr()->remove_player(v18 - 0);
            g_game_ptr()->enable_marky_cam(false, true, -1000.0, 0.0);
            frames_to_skip = 3;
            hero_enabled = hero_enabled_e::SET_TRUE;
        }
        break;
    }
    default:
        break;
    }
    }
    }
}



