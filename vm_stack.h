#pragma once

using vm_num_t = float;
typedef const char * vm_str_t;

struct vm_thread;

inline constexpr auto UNINITIALIZED_SCRIPT_PARM = 0x7BAD05CF;

struct vm_stack {
    int field_0[96];
    char *buffer;
    char *SP;
    vm_thread *my_thread;

    vm_stack(vm_thread *t);

    int capacity() const {
        return 384;
    }

    vm_thread *get_thread() {
        return my_thread;
    }

    int size() const {
        return this->SP - this->buffer;
    }

    vm_num_t& top_num() {
        return *(vm_num_t *)(SP - sizeof(vm_num_t));
    }

    vm_str_t& top_str() {
        return *(vm_str_t*)(SP - sizeof(vm_str_t));
    }



    char *get_SP() const
    {
        return this->SP;
    }

    void set_SP(char *sp) {
        SP = sp;
    }

    void move_SP(int n);

    void pop(int n)
    {
        this->move_SP(-n);
    }

    void* pop_addr()
    {
        this->pop(sizeof(void *));
        return *(void**)SP;
    }

    vm_str_t pop_str()
    {
        this->pop(sizeof(vm_str_t));
        return *(vm_str_t *)SP;
    }

};
