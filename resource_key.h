#pragma once

#include "func_wrapper.h"
#include "string_hash.h"

struct resource_key
{
    string_hash m_hash;
    int m_type;
};

enum resource_key_type {
    RESOURCE_KEY_TYPE_NONE = 0,
    RESOURCE_KEY_TYPE_ANIMATION = 1,
    RESOURCE_KEY_TYPE_NAL_SKL = 2,
    RESOURCE_KEY_TYPE_ALS_FILE = 3,
    RESOURCE_KEY_TYPE_ENTITY = 4,
    RESOURCE_KEY_TYPE_EXTERNAL_ENT = 5,
    RESOURCE_KEY_TYPE_TEXTURE = 6,
    RESOURCE_KEY_TYPE_MPAL_TEXTURE = 7,
    RESOURCE_KEY_TYPE_IFL = 8,
    RESOURCE_KEY_TYPE_DESCRIPTOR = 9,
    RESOURCE_KEY_TYPE_SCN_ENTITY = 10,
    RESOURCE_KEY_TYPE_SCN_AI_SPLINE_PATH = 11,
    RESOURCE_KEY_TYPE_SCN_AUDIO_BOX = 12,
    RESOURCE_KEY_TYPE_SCN_QUAD_PATH = 13,
    RESOURCE_KEY_TYPE_SCN_BOX_TRIGGER = 14,
    RESOURCE_KEY_TYPE_SCRIPT = 15,
    RESOURCE_KEY_TYPE_SCRIPT_INST = 16,
    RESOURCE_KEY_TYPE_NGL_FONT = 17,
    RESOURCE_KEY_TYPE_PANEL = 18,
    RESOURCE_KEY_TYPE_TEXTFILE = 19,
    RESOURCE_KEY_TYPE_ICON = 20,
    RESOURCE_KEY_TYPE_MESH = 21,
    RESOURCE_KEY_TYPE_MORPH = 22,
    RESOURCE_KEY_TYPE_MATERIAL = 23,
    RESOURCE_KEY_TYPE_COLLISION_MESH = 24,
    RESOURCE_KEY_TYPE_PACK = 25,
    RESOURCE_KEY_TYPE_SCENE_ANIM = 26,
    RESOURCE_KEY_TYPE_MISSION_TABLE = 27,

    RESOURCE_KEY_TYPE_SCRIPT_HEADER_FILE = 29,

    RESOURCE_KEY_TYPE_LOD = 40,
    RESOURCE_KEY_TYPE_SIN = 41,
    RESOURCE_KEY_TYPE_SCRIPT_GV = 42,
    RESOURCE_KEY_TYPE_SCRIPT_SV = 43,
    RESOURCE_KEY_TYPE_TOKEN_LIST = 44,
    RESOURCE_KEY_TYPE_DISTRICT_GRAPH = 45,
    RESOURCE_KEY_TYPE_PATH = 46,
    RESOURCE_KEY_TYPE_PATROL_DEF = 47,
    RESOURCE_KEY_TYPE_LANG = 48,
    RESOURCE_KEY_TYPE_SLF = 49,

    RESOURCE_KEY_TYPE_MESH_FILE_STRUCT = 51,
    RESOURCE_KEY_TYPE_MORPH_FILE_STRUCT = 52,
    RESOURCE_KEY_TYPE_MATERIAL_FILE_STRUCT = 53,
};


resource_key create_resource_key_from_path(const char *in_string, int a3)
{
    resource_key key;
    CDECL_CALL(0x004217B0, &key, in_string, a3);

    return key;
}

resource_key calc_resource_string_and_type_from_path(const char* in_string,
    mString* out_string,
    resource_key_type* type_override)
{

    CDECL_CALL(0x004200D0, in_string, out_string, type_override);

}

   // 0x004201E0
void set(resource_key a1, string_hash a2, resource_key_type a3)
{

    CDECL_CALL(0x004201E0, a1,a2, a3);
}


