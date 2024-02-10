#pragma once

#include "entity_base_vhandle.h"
#include "string_hash.h"



struct event_type;
struct script_executable;

struct event_manager {
    //0x004F3BE0
    static void create_inst();

    //0x004EE7A0
    static void clear();

    //0x004F52B0
    static void delete_inst();

    //0x004D1F40
    static event_type *get_event_type(string_hash a1);

    //0x004E19F0
    static event_type *register_event_type(string_hash, bool);

    //0x004D2000
    static bool does_script_have_callbacks(const script_executable *a1);

    //0x004EE9F0
    static void raise_event(string_hash a1, entity_base_vhandle a2);

    //0x004E1B00
    static void garbage_collect();

    static void clear_script_callbacks(entity_base_vhandle a1, const script_executable *a2);

    static inline Var<std::vector<event_type *>> event_types{0x0095BA48};
};

extern void event_manager_patch();
