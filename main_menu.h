#pragma once

#include "femenu.h"

struct main_menu_start : FEMenu {
    int field_2C[61];

    float field_120;
    float field_124;
    bool field_128;
    bool field_129;
    uint16_t field_12A;

    FEMenuSystem* field_12C;


    main_menu_start(FEMenuSystem* a2, int a3, int a4)
        : FEMenu(a2, 0, a3, a4, 8, 0)
    {
        this->field_128 = 0;
        this->field_12C = a2;
        this->field_120 = 0.0;
        this->field_124 = 0.0;
        this->field_12A = 0;
        this->field_128 = 0;
    }
};





