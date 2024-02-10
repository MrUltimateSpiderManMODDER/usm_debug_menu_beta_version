#pragma once

#include "fixedstring.h"
#include "resource_key.h"
#include "resource_pack_location.h"
#include "resource_pack_slot.h"


#include "list.hpp"


#ifdef TEST_CASE
#include <list>
#endif

struct limited_timer;
struct resource_pack_token;

struct resource_pack_queue_entry {
    fixedstring<8> field_0;
    int field_20;



    resource_pack_queue_entry();
};

struct resource_pack_streamer {
    bool active;
    bool currently_streaming;

    std::vector<resource_pack_slot *> *pack_slots;
    resource_key field_8;
    resource_pack_slot *curr_slot;
    int m_slot_index;
    resource_pack_location curr_loc;
    int field_68;

    _std::list<resource_pack_queue_entry> field_6C;

    uint8_t *field_78;
    float field_7C;
    int m_data_size;


    //0x0053E040
    resource_pack_streamer();

    //0x00537C00
    ~resource_pack_streamer();

    inline auto *get_pack_slots() {
        return this->pack_slots;
    }

    inline bool is_active() {
        return active;
    }



    //0x00531B70
    void clear();

    bool can_cancel_load(int a2);

    //0x0051EEB0
    void cancel_load(int a2);



    //0x005560A0
    void flush(void (*a2)(void));

    //0x00551200
    void flush(void (*a2)(void), Float a3);

    //0x005510F0
    void frame_advance(Float a2, limited_timer *a3);

    //0x0051EF70
    void frame_advance_streaming(Float a2);

    //0x0053E150
    void unload_all();

    //0x00550F90


    //0x0054C820
    void frame_advance_idle(Float a2);

    //0x0053E0D0
    void unload(int which_slot_idx);

    //0x00537C60
    void unload_internal(int which_slot_idx);

    //0x0050E120
    void set_active(bool a2);

    bool all_slots_idle();

    //0x0053E1A0
    void finish_data_read();


};

extern void resource_pack_streamer_patch();
