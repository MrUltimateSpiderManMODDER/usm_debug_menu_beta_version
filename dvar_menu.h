#include "debug_menu.h"


#include "debug_menu.h"

struct debug_variable_t;

struct debug_variable_t {
    const char* cameraMinDistStr = "camera_min_dist";
    const char* cameraMaxDistStr = "camera_max_dist";
    const char* cameraSuperMaxDistStr = "camera_supermax_dist";
    debug_variable_t* field_0;
};



debug_variable_t* sub_67086F(debug_variable_t* a1, const char* a2)
{

    return 0;

}

void dvar_select_handler(debug_menu_entry* a1,custom_key_type key_type)
{

    debug_variable_t v5;

    auto script_handler = a1->get_script_handler();
    auto v2 = script_handler.c_str();
    auto* v4 = sub_67086F(&v5, v2);
    auto v6 = 0;
    auto fval = a1->get_fval();
    auto set_fval(v4);
    auto v7 = -1;
}




void populate_dvars_menu(debug_menu_entry* entry)
{

    DWORD* v14[20];

        auto& v1 = entry->text;
        auto* v2 = create_menu(v1, dvar_select_handler, 200);

        auto* v494 = v2;
        entry->set_submenu(v2);

    auto v0 = debug_menu_entry(mString { "base_factor" });
        auto v22 = debug_variable_t { "base_factor" };
        static float base_factorVar = 23.00;
        const float v40[4] = { -1000.0, 1000.0, 0.5, 10.0 };
        v0.set_fl_values(v40);
        v0.set_pt_fval(&base_factorVar);
        v0.set_max_value(1000.0);
        v0.set_min_value(-1000.0);
        v22.field_0;
        v494->add_entry(&v0);

        auto v3 = debug_menu_entry(mString { "camera_max_dist" });
        auto v4 = debug_variable_t { "camera_max_dist" };
        static float cameraMaxDistVar = 4.50;
        const float v32[4] = { -1000.0, 1000.0, 0.5, 10.0 };
        v3.set_fl_values(v32);
        v3.set_pt_fval(&cameraMaxDistVar);
        v3.set_max_value(1000.0);
        v3.set_min_value(-1000.0);
        v4.field_0;
        v494->add_entry(&v3);

        auto v5 = debug_menu_entry(mString { "camera_min_dist" });
        auto v6 = debug_variable_t { "camera_min_dist" };
        static float cameraMinDistVar = 2.25;
        const float v82[4] = { -1000.0, 1000.0, 0.5, 10.0 };
        v5.set_fl_values(v82);
        v5.set_pt_fval(&cameraMinDistVar);
        v5.set_max_value(1000.0);
        v5.set_min_value(-1000.0);
        v6.field_0;
        v494->add_entry(&v5);

        auto v7 = debug_menu_entry(mString { "camera_supermax_dist" });
        auto v8 = debug_variable_t { "camera_supermax_dist" };
        static float cameraSuperMaxDistVar = 8.00;
        const float v52[4] = { -1000.0, 1000.0, 0.5, 10.0 };
        v7.set_fl_values(v52);
        v7.set_pt_fval(&cameraSuperMaxDistVar);
        v7.set_max_value(1000.0);
        v7.set_min_value(-1000.0);
        v8.field_0;
        v494->add_entry(&v7);

        auto v9 = debug_menu_entry(mString { "jump_cap_vel" });
        auto v10 = debug_variable_t { "jump_cap_vel" };
        static float jump_cap_vel = 33.00;
        const float v62[4] = { -1000.0, 1000.0, 0.5, 10.0 };
        v9.set_fl_values(v62);
        v9.set_pt_fval(&jump_cap_vel);
        v9.set_max_value(1000.0);
        v9.set_min_value(-1000.0);
        v10.field_0;
        v494->add_entry(&v9);

        auto v11 = debug_menu_entry(mString { "snow_balling" });
        auto v12 = debug_variable_t { "jump_cap_vel" };
        static float snow_balling = 2.50;
        const float v72[4] = { -1000.0, 1000.0, 0.5, 10.0 };
        v9.set_fl_values(v72);
        v11.set_pt_fval(&snow_balling);
        v11.set_max_value(1000.0);
        v12.field_0;
        v494->add_entry(&v11);
    }

void create_dvars_menu(debug_menu* parent);

void create_dvars_menu(debug_menu* parent)
{
    assert(parent != nullptr);

    auto v5 = debug_menu_entry(mString { "Dvars" });
    parent->add_entry(&v5);

    debug_menu_entry* entry = &parent->entries[parent->used_slots - 1];
    populate_dvars_menu(entry);
}

