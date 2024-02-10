#pragma once

#include "variable.h"
#include "wds.h"
#include "mission_table_container.h"
#include "mission_manager_script_data.h"

#include <cstdio>

struct mission_manager {
    int field_0;
    int field_4;
    int field_8;
    int field_C;
    int field_10;
    mission_table_container *m_global_table_container;
    mission_table_container *m_district_table_containers[1];
    int field_1C;
    int field_20;
    int field_24;
    int field_28;
    int field_2C;
    int field_30;
    int field_34;
    int m_district_table_count;
    mission_manager_script_data *m_script_to_load;
    mission_manager_script_data *m_script;
    int field_44[3];
    bool m_unload_script;
    int field_54;
    int field_58;
    int field_5C;
    float *field_60;
    float field_64;
    int field_68;
    float *field_6C;
    int field_70;
    int field_74;
    float *field_78;
    float *field_7C;
    bool field_80;
    int field_84;
    fixedstring32 field_88;
    fixedstring32 field_A8;
    int field_C8;
    bool field_CC;
    fixedstring32 field_D0;
    int hero_switch_frame;
    float field_F4;
    float field_F8;
    int field_FC;

    //0x005DA010
    mission_manager();

    int get_district_table_count() const
    {
        return this->m_district_table_count;
    }

    mission_table_container *get_district_table(int a2)
    {
        return this->m_district_table_containers[a2];
    }

    mission_table_container *get_global_table()
    {
        return this->m_global_table_container;
    }

    void prepare_unload_script()
    {
        if ( this->m_script != nullptr )
        {
            if ( !this->m_unload_script )
            {
                auto *v1 = this->m_script->field_0.c_str();
                printf("Preparing to unload script %s\n", v1);
                this->m_unload_script = true;
                this->field_80 = false;
            }
        }
    }

        void force_mission(int a2, const char* a3, int a4, const char* a5)
    {
        this->prepare_unload_script();
        this->field_80 = true;
        this->field_84 = a2;

        fixedstring32 v5 { a3 };
        this->field_88 = v5;
        this->field_C8 = a4;
        if (a5 != nullptr) {
            fixedstring32 v6 { a5 };
            this->field_A8 = v6;
        } else {
            fixedstring32 v7 { "" };
            this->field_A8 = v7;
        }
    }

    void force_mission2(int a2, const char* a3, int a4, const char* a5)
    {
        void(__fastcall * func)(void*, void*, int, const char*, int, const char*) = bit_cast<decltype(func)>(0x005C5A00);

        func(this, nullptr,a2,a3,a4,a5);
    }

    void sort_district_priorities()
    {
        CDECL_CALL(0x005DBCD0, this);
    }

    bool get_script(mission_manager_script_data* return_script_data)
    {
        return (bool)CDECL_CALL(0x005E13D0, this, return_script_data);
    }

    int add_global_table(const resource_key& a2)
    {
        return CDECL_CALL(0x005D1EA0, this, &a2);
    }

    


    void load_script(const mission_manager_script_data& data)
{
            void(__fastcall * func)(void*, void*, const mission_manager_script_data&) = bit_cast<decltype(func)>(0x005DEE40);

            return func(this, nullptr, data);
}

        void run_script(const mission_manager_script_data& arg0)
{
            void(__fastcall * func)(void*, void*, const mission_manager_script_data&) = bit_cast<decltype(func)>(0x005DEFA0);

            return func(this, nullptr, arg0);
}

    void frame_advance(Float a2)
{
            void(__fastcall * func)(void*, void*, Float) = bit_cast<decltype(func)>(0x005E16B0);

            return func(this, nullptr, a2);
}

    void unload_script_if_requested()
{

         void(__fastcall * func)(void*, void*) = bit_cast<decltype(func)>(0x005DBD00);

         return func(this, nullptr);
}

        void kill_braindead_script()
{

         void(__fastcall * func)(void*, void*) = bit_cast<decltype(func)>(0x005D7EF0);

         return func(this, nullptr);
}








    void show_mission_loading_panel(const mString& a1)
{
            void(__fastcall * func)(void*, void*, mString) = bit_cast<decltype(func)>(0x005DA4B0);

            return func(this, nullptr, a1);
}

   


    static inline Var<mission_manager *> s_inst{0x00968518};
};

