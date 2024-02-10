#pragma once


#include <cstdint>

struct fixed_pool {
    int field_0;
    void *field_4;
    int m_size;
    uint32_t m_alignment;
    int m_number_of_entries_per_block;
    int field_14;
    int field_18;
    void *m_base;
    bool m_initialized;

    fixed_pool(int a2, int a3, int a4, int a5, int a6, void* base)
    {
        this->field_0 = 0;
        this->init(a2, a3, a4, a5, a6, base);
    }

    void* sub_501DD0()
    {
        return (void*)CDECL_CALL(0x501DD0);
    }

void init(int size, int a3, int a4, int a5, int a6, void* base)
    {
        printf("%d", size);
        this->m_size = size;
        this->m_alignment = a4;
        this->m_number_of_entries_per_block = a3;
        this->field_18 = a5;

        this->m_base = (base != nullptr ? base : sub_501DD0());

        assert(m_alignment >= 4);
        assert(m_number_of_entries_per_block >= 1);

        auto v8 = this->m_size / this->m_alignment;
        if ((this->m_size & (this->m_alignment - 1)) != 0) {
            ++v8;
        }

        this->m_size = this->m_alignment * v8;
        assert(m_size > 0);

        this->field_14 = 0;
        this->field_4 = nullptr;
        this->m_initialized = true;
        for (auto i = 0; i < a6; ++i) {
            this->sub_4368C0();
        }
    }

void sub_4368C0()
    {

            CDECL_CALL(0x4368C0, this);
        }
    
    
    int get_entry_size() const
    {
        return this->m_size;
    }

    void* allocate_new_block()
    {
        assert(m_initialized);

        if (!this->field_4) {
            if (!this->field_18 && this->field_14 > 0) {
                return nullptr;
            }

            this->sub_4368C0();
        }

        auto v2 = (void**)this->field_4;
        this->field_4 = *v2;
        return v2;
    }
};

template<typename T>
void *allocate_new_block(fixed_pool &pool);
