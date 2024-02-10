#pragma once

#include "func_wrapper.h"
#include "resource_key.h"
#include "resource_partition.h"
#include "memory.h"

#include "os_file.h"
#include "resource_amalgapak_header.h"


struct resource_partition;
struct resource_pack_location;
struct resource_memory_map;
struct mString;

Var<int> amalgapak_base_offset { 0x00921CB4 };

Var<int> resource_buffer_used { 0x0095C180 };

Var<int> memory_maps_count { 0x0095C7F4 };

Var<size_t> resource_buffer_size { 0x0095C1C8 };

Var<int> in_use_memory_map { 0x00921CB0 };

Var<uint8_t*> resource_buffer { 0x0095C738 };

Var<bool> using_amalga { 0x0095C800 };

Var<int> amalgapak_signature { 0x0095C804 };

Var<resource_memory_map*> memory_maps { 0x0095C2F0 };

Var<int> amalgapak_pack_location_count { 0x0095C7FC };

Var<resource_pack_location*> amalgapak_pack_location_table { 0x0095C7F8 };

Var<int> amalgapak_prerequisite_count { 0x0095C174 };

Var<resource_key*> amalgapak_prerequisite_table { 0x0095C300 };



namespace resource_manager {

    void load_amalgapak()
    {
        CDECL_CALL(0x00537650);
    }




    bool is_idle()
    {
      return (bool)CDECL_CALL(0x00537AC0);
   }
    
    bool using_amalgapak()
   {
      return using_amalga();
   }

    bool can_reload_amalgapak()
    {

            return (bool)CDECL_CALL(0x0053DE90);
        }
    
void reload_amalgapak()
        {
            printf("resource_manager::reload_amalgapak");

            if constexpr (1) {
                assert(!using_amalgapak());

                assert(amalgapak_pack_location_table() != nullptr);

                assert(amalgapak_prerequisite_table() != nullptr);

                assert(memory_maps() != nullptr);

                mem_freealign(amalgapak_prerequisite_table());
                mem_freealign(amalgapak_pack_location_table());
                delete[] (memory_maps());
                amalgapak_prerequisite_table() = nullptr;
                amalgapak_pack_location_table() = nullptr;
                memory_maps() = nullptr;
                load_amalgapak();

                std::vector<resource_key> v3;
                for (auto i = 0; i < amalgapak_pack_location_count(); ++i) {
                    if (amalgapak_pack_location_table()[i].field_2C != 0) {
                        v3.push_back(amalgapak_pack_location_table()[i].loc.field_0);
                    }
                }

                for (auto& cb : resource_pack_modified_callbacks()) {
                    (*cb)(v3);
                }
            } else {
                CDECL_CALL(0x0054C2E0);
            }
        }

       void update_hero_switch()
    {
        void(__fastcall * func)(void*) = (decltype(func))0x005C5790;
        func(nullptr);
    }

    resource_pack_slot* get_best_context(resource_partition_enum a1)
    {

    void(__fastcall * func)(void*) = (decltype(func))0x00537610;
    func(nullptr);

    }
    bool get_pack_file_stats(const resource_key &a1,
                                    resource_pack_location *a2,
                                    mString *a3,
                                    int *a4)
    {

        return (bool) CDECL_CALL(0x0052A820, &a1, a2, a3, a4);
    }

        resource_pack_slot* push_resource_context(resource_pack_slot* pack_slot)
    {
        return (resource_pack_slot*)CDECL_CALL(0x00542740, pack_slot);
    }

    resource_pack_slot* get_and_push_resource_context(resource_partition_enum a1)
    {
        auto* v1 = get_best_context(a1);
        return push_resource_context(v1);
    }




    resource_pack_slot *pop_resource_context() {
        return (resource_pack_slot *) CDECL_CALL(0x00537530);
    }

    void set_active_district(bool a1)
    {
        void (__fastcall *func)(void *, int, bool) = (decltype(func)) 0x00573620;
        func(nullptr, 0, a1);
    }

    resource_partition *get_partition_pointer(int which_type) {
        return (resource_partition *) CDECL_CALL(0x00537AA0, which_type);
    }

}
