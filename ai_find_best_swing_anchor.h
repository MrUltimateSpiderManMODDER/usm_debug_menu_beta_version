#pragma once

#include "float.hpp"

struct vector3d;
struct entity;
struct find_best_anchor_result_t;

struct quick_anchor_info;
struct sweet_cone_t;

namespace local_collision {
struct primitive_list_t;
}

struct swing_anchor_finder {
    float field_0;
    float field_4;
    float sweet_spot_distance;
    float field_C;
    float field_10;
    float web_min_length;
    float web_max_length;
    float field_1C;
    float field_20;

    swing_anchor_finder() = default;

    swing_anchor_finder(Float a1, Float a2, Float a3, Float a4, Float a5, Float a6);

    void set_max_pull_length(Float length);

    //0x00464590
    bool accept_swing_point(const quick_anchor_info &info,
                            const sweet_cone_t &sweet_cone,
                            const Float &a4,
                            Float a5,
                            Float *arg10,
                            local_collision::primitive_list_t **a7,
                            local_collision::primitive_list_t ***a8) const;

    //0x00486280
    bool find_best_offset_anchor(entity *self,
                                 const vector3d &a3,
                                 const vector3d &a4,
                                 find_best_anchor_result_t *result);

    //0x00487D20
    bool find_best_anchor(entity *a1, const vector3d &a2, find_best_anchor_result_t *a3);

    static void remove_all_anchors();
};

extern void swing_anchor_finder_patch();
