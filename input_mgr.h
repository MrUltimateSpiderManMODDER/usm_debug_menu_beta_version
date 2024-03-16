#pragma once

#include "variable.h"
#include "common.h"


struct device_id_t {
    int field_0;

    };


    struct input_mgr
{
    int field_0;
    struct {

        void disable_vibration()
        {
            void (__fastcall *func)(void *) = (decltype(func)) 0x005C5440;
            func(this);
        }

        void enable_vibration()
        {
            void (__fastcall *func)(void *) = (decltype(func)) 0x005C5430;
            func(this);
        }

        float get_control_state(int a2, device_id_t a3) const

        {
            void(__fastcall * func)(void*, int, device_id_t) = (decltype(func))0x005D86D0;
            func(0, a2, a3);
        }

    } *rumble_ptr;


    int field_8[10];
    void *field_30[1];

    static inline Var<input_mgr *> instance{0x009685DC};
};

VALIDATE_OFFSET(input_mgr, field_30, 0x30);
