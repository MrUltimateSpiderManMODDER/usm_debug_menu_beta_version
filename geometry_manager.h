#pragma once

#include "func_wrapper.h"

namespace geometry_manager {
void enable_scene_analyzer(bool a1)
{
    CDECL_CALL(0x00515730, a1);
}

bool is_scene_analyzer_enabled()
{
    return (bool)CDECL_CALL(0x00515720);
}

void set_field_of_view(Float fov);

void set_field_of_view(Float fov)
{
    CDECL_CALL(0x0053FFF0);
}

    
}
