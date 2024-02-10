#pragma once

#include "func_wrapper.h"
#include "log.h"
#include "string_hash_entry.h"
#include "mcontainer.h"

#include <type_traits>

struct from_mash_in_place_constructor;
struct mash_info_struct;

template<typename T>
struct mAvlNode {
    using value_type = T;

    T *m_key;
    mAvlNode<T> *m_left;
    mAvlNode<T> *m_right;
    mAvlNode<T> *m_parent;
    char field_10;

    explicit mAvlNode(T *a2) {
        this->field_10 = 0;
        this->m_parent = nullptr;
        this->m_right = nullptr;
        this->m_left = nullptr;
        this->m_key = a2;
    }
};

template<typename T>
struct mAvlTree : mContainer {
    struct iterator {
        mAvlNode<string_hash_entry> *field_0;

        //0x00743470
        void iterate() {
#if 1
            auto *v1 = this->field_0->m_right;
            if (v1 != nullptr) {
                this->field_0 = v1;
                if (v1->m_left != nullptr) {
                    mAvlNode<string_hash_entry> *v2;
                    do {
                        v2 = this->field_0->m_left;
                        this->field_0 = v2;
                    } while (v2->m_left != nullptr);
                }
            } else {
                if (this->field_0->m_parent) {
                    mAvlNode<string_hash_entry> *v3;
                    do {
                        v3 = this->field_0->m_parent;
                        if (this->field_0 != v3->m_right) {
                            break;
                        }

                        this->field_0 = v3;
                    } while (v3->m_parent != nullptr);
                }

                this->field_0 = this->field_0->m_parent;
            }
#else
            THISCALL(0x00743470, this);
#endif
        }
    };

    using value_type = T;
    using node_type = mAvlNode<T>;

    node_type *m_head;
    char field_C;

    mAvlTree() {
        sub_420EE0();
        this->m_head = nullptr;
        this->m_size = 0;
        this->field_C = 1;
    }

    mAvlTree(int a1) : mContainer(a1) {}

    mAvlTree(from_mash_in_place_constructor *a2) : mContainer(a2) {}

    void sub_420EE0() {
        this->m_size = 0;
        this->field_0 = 0;
    }

    inline T *find(T *a1) {
        auto v1 = findHelper(this->m_head, a1);
        if (v1 != nullptr) {
            return v1->m_key;
        }

        return nullptr;
    }

    node_type *findHelper(node_type *a1, T *a2) {
        if (a1 == nullptr || a2 == nullptr) {
            return nullptr;
        }

        auto v2 = a1->m_key->field_0.source_hash_code;

        int v5;
        if (v2 <= a2->field_0.source_hash_code) {
            v5 = -(v2 < a2->field_0.source_hash_code);
        } else {
            v5 = 1;
        }

        int v6 = -v5;
        if (v6 != 0) {
            if (v6 < 0) {
                return this->findHelper(a1->m_left, a2);
            }

            if (v6 > 0) {
                return this->findHelper(a1->m_right, a2);
            }

            return nullptr;
        }

        return a1;
    }

    void sub_439AD0(mAvlNode<T> *a1);

    void sub_43BF70(mAvlNode<T> **a1);

    void sub_744960(mAvlNode<T> **a1);

    void sub_564370(mAvlNode<T> **a1);

    void sub_745DF0(mAvlNode<T> **a2);

    int *sub_64A090(int *a2);

    void sub_64A2B0();

    //0x0056DF00 -> T = T = string_hash_entry
    bool insert(T *new_element) {
        assert(new_element != nullptr);
        node_type *v4 = new node_type {new_element};

        int v5 = this->addHelper(v4, this->m_head, nullptr);
        bool v6 = (v5 != 0);
        if (v5 == 0 && v4 != nullptr) {
            operator delete(v4);
        }

        return v6;
    }

    int addHelper(node_type *a2, node_type *&a3, node_type *a4);

    void unmash(mash_info_struct *a2, void *a3);

    //0x005707B0
    void insert_tree(mAvlTree<string_hash_entry> *a2) {
        THISCALL(0x005707B0, this, a2);
    }

    void sub_5702D0() {
        auto *v2 = this->m_head;
        auto **v3 = &this->m_head;
        if (v2 != nullptr) {
            this->dump(v2->m_left);
            this->dump(v2->m_right);
            this->destroy_element(v3);
            --this->m_size;
        }
    }

    void dump(mAvlNode<string_hash_entry> *&node) {
        if (node != nullptr) {
            dump(node->m_left);
            dump(node->m_right);
            destroy_element(&node);
            --this->m_size;
        }
    }

    void destruct_mashed_class() {
        dump(m_head);
    }

    void destroy_element(mAvlNode<string_hash_entry> **p_node) {
        THISCALL(0x00567350, this, p_node);
    }
};

extern void mAvlTree_patch();
