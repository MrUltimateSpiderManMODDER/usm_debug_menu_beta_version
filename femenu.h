#pragma once

#include "float.hpp"

#include <cstdint>

struct FEMenuSystem;
struct FEText;
struct FEMenuEntry;

struct FEMenu {
    std::intptr_t m_vtbl;
    FEMenuEntry **field_4;
    FEMenuSystem *field_8;
    int field_C;
    int field_10;
    int field_14;
    int field_18;
    float field_1C;
    int16_t field_20;
    int16_t highlighted;
    int16_t num_entries;
    int16_t field_26;
    int16_t field_28;
    char field_2A;
    char field_2B;

    //0x0060AA90
    FEMenu(FEMenuSystem *a2, uint32_t a3, int a4, int a5, int16_t a6, int16_t a7);

    void sub_60B180(Float a2);

    //virtual
    void Load();

    //0x00618610
    //virtual
    void AddEntry(int a2, FEText *a3, bool a4);

    /* virtual */ void AddEntry(int a2, global_text_enum a3);

    /* virtual */ void Init();

    /* virtual */ void Update(Float a2);

    /* virtual */ void OnActivate();

    /* virtual */ void OnDeactivate(FEMenu *a2);

    /* virtual */ void OnAnyButtonPress(int a2, int a3);

    /* virtual */ void OnButtonRelease(int a2, int a3);

    /* virtual */ void SetHigh(int a2, bool a3);

    /* virtual */ void SetVis(int a2);

    /* virtual */ void Up();

    /* virtual */ void Down();

    /* virtual */ void ButtonHeldAction();
};

extern void sub_582A30();

extern void sub_5A6D70();

extern void FEMenu_patch();
