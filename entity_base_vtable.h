#pragma once

#include <cstdint>

struct entity_base_vtable {
    std::intptr_t m_vtbl;

    entity_base_vtable();

    ~entity_base_vtable() = default;

    // int get_entity_size() = 0;
};
