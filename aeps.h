#pragma once

#include "variable.h"

#include "float.hpp"

namespace aeps {
    //0x004D3980
    void FrameAdvance(Float a1);

    //0x004CDFC0
    void RefreshDevOptions();

    extern Var<void *> s_activeStructs;

    //0x004D91A0
    void Reset();

    void Init();
};

extern void aeps_patch();
