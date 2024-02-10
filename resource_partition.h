#pragma once

#include "resource_pack_slot.h"
#include "resource_pack_streamer.h"

enum resource_partition_enum {
    RESOURCE_PARTITION_START = 0,
    RESOURCE_PARTITION_HERO = 1,
    RESOURCE_PARTITION_LANG = 2,
    RESOURCE_PARTITION_MISSION = 3,
    RESOURCE_PARTITION_COMMON = 4,
    RESOURCE_PARTITION_STRIP = 5,
    RESOURCE_PARTITION_DISTRICT = 6,
    RESOURCE_PARTITION_STANDALONE = 7,
    RESOURCE_PARTITION_END = 8,
};

using modified_callback_t = void (*)(std::vector<resource_key>& a1);


static inline Var<std::vector<modified_callback_t>> resource_pack_modified_callbacks { 0x0095CAC4 };

struct resource_partition
{
    int field_0;
    int field_4;

    struct {
        int field_0;
        resource_pack_slot **m_first;
        resource_pack_slot **m_last;
        resource_pack_slot **m_end;

        struct iterator
        {
            resource_pack_slot **m_ptr;

            auto &operator*() const
            {
                return (*m_ptr);
            }
        };

        struct resource_memory_map {
            char field_0;
            int field_4;
            int field_8;
            int field_C;
            struct {
                int field_0;
                int field_4;
                int field_8;
                int field_C;
            } field_10[8];

            resource_memory_map()
            {
                this->field_0 = 0;
                std::memset(this->field_10, 0, sizeof(this->field_10));
            }
        };

        iterator begin()
        {
            return iterator{m_first};
        }


        resource_pack_streamer streamer;

            resource_pack_streamer* get_streamer()
        {
            return (&streamer);
        }

        iterator end()
        {
            return iterator{m_last};
        }

        size_t size() const
        {
            return (m_first == nullptr ? 0 : m_last - m_first);
        }

        auto &front() {
            return (*begin());
        }

    } m_pack_slots;

    auto &get_pack_slots() {
        return this->m_pack_slots;
    }
};
