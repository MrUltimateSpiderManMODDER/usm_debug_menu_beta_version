#pragma once

#include "entity_base_vtable.h"

#include "entity_base_vhandle.h"
#include "string_hash.h"
#include "vector3d.h"



struct po;
struct event;
struct conglomerate;
struct generic_mash_header;
struct generic_mash_data_ptrs;
struct mString;
struct time_interface;
struct sound_and_pfx_interface;
struct chunk_file;
struct damage_interface;
struct facial_expression_interface;
struct physical_interface;
struct skeleton_interface;
struct animation_interface;
struct script_data_interface;
struct decal_data_interface;
struct resource_key;
struct motion_effect_struct;



struct entity_base : entity_base_vtable {
    uint32_t field_4;
    uint32_t field_8;
    po *my_rel_po;
    string_hash field_10;
    po *my_abs_po;
    motion_effect_struct *field_18;
    entity_base_vhandle my_handle;
    entity_base *m_parent;
    entity_base *m_child;
    entity_base *field_28;
    int16_t proximity_map_cell_reference_count;
    uint8_t m_timer;
    std::vector<entity_base *> *adopted_children;
    conglomerate *my_conglom_root;
    sound_and_pfx_interface *my_sound_and_pfx_interface;
    int16_t field_3C;
    int16_t field_3E;
    int8_t field_40;
    int8_t field_41;
    int8_t rel_po_idx;
    int8_t proximity_map_reference_count;



    bool is_a_game_camera()
    {
        if constexpr (1) {
            auto& func = get_vfunc(m_vtbl, 0x74);

            return (bool)func(this);

        } else {
            return false;
        }
    }


};


