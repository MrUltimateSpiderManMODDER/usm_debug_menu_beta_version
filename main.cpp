#ifdef _WIN32
#define _USE_MATH_DEFINES
#endif

#include <cmath>

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <vector>
#include <string>
#include <algorithm>
#include <iostream>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <stdint.h>
#pragma comment(lib, "Dinput8.lib")
#pragma comment(lib, "Dxguid.lib")


#include "slf_functions.h"
#include "devopt.h"
#include "dvar_menu.h"
#include "sound_manager.h"
#include "entity_animation_menu.h"
#include "game.h"
#include "input_mgr.h"
#include "resource_manager.h"
#include "forwards.h"
#include "variable.h"
#include "func_wrapper.h"
#include "fixedstring32.h"
#include "levelmenu.h"
#include "memory_menu.h"
#include "mission_manager.h"
#include "mission_table_container.h"
#include "mstring.h"
#include "region.h"
#include "debug_menu.h"
#include "os_developer_options.h"
#include "script_executable.h"
#include "script_library_class.h"
#include "script_object.h"
#include "spider_monkey.h"
#include "geometry_manager.h"
#include "entity.h"
#include "terrain.h"
#include "debug_menu_extra.h"
#include "app.h"
#include "main_menu.h"
#include "pausemenusystem.h"


DWORD* entity_variants_current_player = nullptr;
DWORD* fancy_player_ptr = nullptr;

debug_menu_entry* g_debug_camera_entry = nullptr;

/*
#undef IsEqualGUID
BOOL WINAPI IsEqualGUID(
	REFGUID rguid1,
	REFGUID rguid2)
{
	return !memcmp(rguid1, rguid2, sizeof(GUID));
}
*/

uint8_t color_ramp_function(float ratio, int period_duration, int cur_time) {

	if (cur_time <= 0 || 4 * period_duration <= cur_time)
		return 0;

	if (cur_time < period_duration) {

		float calc = ratio * cur_time;

		return min(calc, 1.0f) * 255;
	}


	if (period_duration <= cur_time && cur_time <= 3 * period_duration) {
		return 255;
	}


	if (cur_time <= 4 * period_duration) {

		int adjusted_time = cur_time - 3 * period_duration;
		float calc = 1.f - ratio * adjusted_time;

		return min(calc, 1.0f) * 255;
	}

    return 0;

}

static void *HookVTableFunction(void *pVTable, void *fnHookFunc, int nOffset) {
    intptr_t ptrVtable = *((intptr_t *) pVTable); // Pointer to our chosen vtable
    intptr_t ptrFunction = ptrVtable +
        sizeof(intptr_t) *
            nOffset; // The offset to the function (remember it's a zero indexed array with a size of four bytes)
    intptr_t ptrOriginal = *((intptr_t *) ptrFunction); // Save original address

    // Edit the memory protection so we can modify it
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery((LPCVOID) ptrFunction, &mbi, sizeof(mbi));
    VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &mbi.Protect);

    // Overwrite the old function with our new one
    *((intptr_t *) ptrFunction) = (intptr_t) fnHookFunc;

    // Restore the protection
    VirtualProtect(mbi.BaseAddress, mbi.RegionSize, mbi.Protect, &mbi.Protect);

    // Return the original function address incase we want to call it
    return (void *) ptrOriginal;
}

typedef struct _list{
	struct _list* next;
	struct _list* prev;
	void* data;
}list;

static constexpr DWORD MAX_ELEMENTS_PAGE = 18;

debug_menu* script_menu = nullptr;
debug_menu* progression_menu = nullptr;
debug_menu* entity_variants_menu = nullptr;
debug_menu* extra_menu = nullptr;


debug_menu** all_menus[] = {
    &debug_menu::root_menu,
    &script_menu,
    &progression_menu,
    &entity_variants_menu,
    &extra_menu,

};

void unlock_region(region* cur_region) {
	cur_region->flags &= 0xFE;
}

void remove_debug_menu_entry(debug_menu_entry* entry) {
	
	DWORD to_be = (DWORD)entry;
	for (auto i = 0u; i < (sizeof(all_menus) / sizeof(void*)); ++i) {

		debug_menu *cur = *all_menus[i];

		DWORD start = (DWORD) cur->entries;
		DWORD end = start + cur->used_slots * sizeof(debug_menu_entry);

		if (start <= to_be && to_be < end) {

			int index = (to_be - start) / sizeof(debug_menu_entry);

			memcpy(&cur->entries[index], &cur->entries[index + 1], cur->used_slots - (index + 1));
			memset(&cur->entries[cur->used_slots - 1], 0, sizeof(debug_menu_entry));
			cur->used_slots--;
			return;
		}
		
	}

	printf("FAILED TO DEALLOCATE AN ENTRY :S %08X\n", entry);

}

int vm_debug_menu_entry_garbage_collection_id = -1;

typedef int (*script_manager_register_allocated_stuff_callback_ptr)(void* func);
script_manager_register_allocated_stuff_callback_ptr script_manager_register_allocated_stuff_callback = (script_manager_register_allocated_stuff_callback_ptr) 0x005AFE40;

typedef int (*construct_client_script_libs_ptr)();
construct_client_script_libs_ptr construct_client_script_libs = (construct_client_script_libs_ptr) 0x0058F9C0;

void vm_debug_menu_entry_garbage_collection_callback(void* a1, list* lst) {

	list* end = lst->prev;

	for (list* cur = end->next; cur != end; cur = cur->next) {

		debug_menu_entry* entry = ((debug_menu_entry*)cur->data);
		//printf("Will delete %s %08X\n", entry->text, entry);
		remove_debug_menu_entry(entry);
	}
}

int construct_client_script_libs_hook() {

	if (vm_debug_menu_entry_garbage_collection_id == -1) {
		int res = script_manager_register_allocated_stuff_callback((void *) vm_debug_menu_entry_garbage_collection_callback);
		vm_debug_menu_entry_garbage_collection_id = res;
	}

	return construct_client_script_libs();
}

region** all_regions = (region **) 0x0095C924;
DWORD* number_of_allocated_regions = (DWORD *) 0x0095C920;

typedef const char* (__fastcall* region_get_name_ptr)(void* );
region_get_name_ptr region_get_name = (region_get_name_ptr) 0x00519BB0;

typedef int (__fastcall *region_get_district_variant_ptr)(region* );
region_get_district_variant_ptr region_get_district_variant = (region_get_district_variant_ptr) 0x005503D0;

void set_text_writeable() {

	const DWORD text_end = 0x86F000;
	const DWORD text_start = 0x401000;

	DWORD old;
	VirtualProtect((void*)text_start, text_end - text_start, PAGE_EXECUTE_READWRITE, &old);
}

void set_rdata_writeable() {

	const DWORD end = 0x91B000;
	const DWORD start = 0x86F564;

	DWORD old;
	VirtualProtect((void*)start, end - start, PAGE_READWRITE, &old);
}

void HookFunc(DWORD callAdd, DWORD funcAdd, BOOLEAN jump, const char* reason) {

	//Only works for E8/E9 hooks	
	DWORD jmpOff = funcAdd - callAdd - 5;

	BYTE shellcode[] = { 0, 0, 0, 0, 0 };
	shellcode[0] = jump ? 0xE9 : 0xE8;

	memcpy(&shellcode[1], &jmpOff, sizeof(jmpOff));
	memcpy((void*)callAdd, shellcode, sizeof(shellcode));

	if (reason)
		printf("Hook: %08X -  %s\n", callAdd, reason);

}


void WriteDWORD(int address, DWORD newValue, const char* reason) {
	* ((DWORD *)address) = newValue;
	if (reason)
		printf("Wrote: %08X -  %s\n", address, reason);
}

typedef int (*nflSystemOpenFile_ptr)(HANDLE* hHandle, LPCSTR lpFileName, unsigned int a3, LARGE_INTEGER liDistanceToMove);
nflSystemOpenFile_ptr nflSystemOpenFile_orig = nullptr;

nflSystemOpenFile_ptr* nflSystemOpenFile_data = (nflSystemOpenFile_ptr *) 0x0094985C;

HANDLE USM_handle = INVALID_HANDLE_VALUE;

int nflSystemOpenFile(HANDLE* hHandle, LPCSTR lpFileName, unsigned int a3, LARGE_INTEGER liDistanceToMove) {

	//printf("Opening file %s\n", lpFileName);
	int ret = nflSystemOpenFile_orig(hHandle, lpFileName, a3, liDistanceToMove);


	if (strstr(lpFileName, "ultimate_spiderman.PCPACK")) {

	}
	return ret;
}

typedef int (*ReadOrWrite_ptr)(int a1, HANDLE* a2, int a3, DWORD a4, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite);
ReadOrWrite_ptr* ReadOrWrite_data = (ReadOrWrite_ptr *)0x0094986C;
ReadOrWrite_ptr ReadOrWrite_orig = nullptr;

int ReadOrWrite(int a1, HANDLE* a2, int a3, DWORD a4, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite) {

	int ret = ReadOrWrite_orig(a1, a2, a3, a4, lpBuffer, nNumberOfBytesToWrite);

	if (USM_handle == *a2) {
		printf("USM buffer was read %08X\n", (DWORD)lpBuffer);


	}
	return ret;
}

typedef void (*aeps_RenderAll_ptr)();
aeps_RenderAll_ptr aeps_RenderAll_orig = (aeps_RenderAll_ptr)0x004D9310;


unsigned int nglColor(int r, int g, int b, int a)
{
    return ( (a << 24) |  (r << 16) | (g << 8) | (b & 255) );
}



void aeps_RenderAll() {
	static int cur_time = 0;
	int period = 60;
	int duration = 6 * period;
	float ratio = 1.f / period;

	uint8_t red = color_ramp_function(ratio, period, cur_time + 2 * period) + color_ramp_function(ratio, period, cur_time - 4 * period);
	uint8_t green = color_ramp_function(ratio, period, cur_time);
	uint8_t blue = color_ramp_function(ratio, period, cur_time - 2 * period);

	nglListAddString(*nglSysFont, 0.1f, 0.2f, 0.2f, nglColor(red, green, blue, 255), 1.f, 1.f, "");

	cur_time = (cur_time + 1) % duration;


	aeps_RenderAll_orig();
}


uint32_t keys[256];

void getStringDimensions(const char* str, int* width, int* height) {
	nglGetStringDimensions(*nglSysFont, str, width, height, 1.0, 1.0);
}

int getStringHeight(const char* str) {
	int height;
	nglGetStringDimensions(nglSysFont(), str, nullptr, &height, 1.0, 1.0);
	return height;
}

std::string getRealText(debug_menu_entry *entry) {
    assert(entry->render_callback != nullptr);

    auto v1 = entry->render_callback(entry);

    char a2a[256]{};
    if (v1.size() != 0) {
        auto *v7 = v1.c_str();
        auto *v4 = entry->text;
        snprintf(a2a, 255u, "%s: %s", v4, v7);
    } else {
        auto *v5 = entry->text;
        snprintf(a2a, 255u, "%s", v5);
    }

    return {a2a};
}

void render_current_debug_menu()
{
    auto UP_ARROW { " ^ ^ ^ " };
    auto DOWN_ARROW { " v v v " };

    int num_elements = min((DWORD)MAX_ELEMENTS_PAGE, current_menu->used_slots - current_menu->window_start);
    int needs_down_arrow = ((current_menu->window_start + MAX_ELEMENTS_PAGE) < current_menu->used_slots) ? 1 : 0;

    int cur_width, cur_height;
    int debug_width = 0;
    int debug_height = 0;

    auto get_and_update = [&](auto* x) {
        getStringDimensions(x, &cur_width, &cur_height);
        debug_height += cur_height;
        debug_width = max(debug_width, cur_width);
    };

    // printf("new size: %s %d %d (%d %d)\n", x, debug_width, debug_height, cur_width, cur_height);

    get_and_update(current_menu->title);
    get_and_update(UP_ARROW);

    int total_elements_page = needs_down_arrow ? MAX_ELEMENTS_PAGE : current_menu->used_slots - current_menu->window_start;

    for (int i = 0; i < total_elements_page; ++i) {
        debug_menu_entry* entry = &current_menu->entries[current_menu->window_start + i];
        auto cur = getRealText(entry);
        get_and_update(cur.c_str());
    }

    if (needs_down_arrow) {
        get_and_update(DOWN_ARROW);
    }

    nglQuad quad;

    int menu_x_start = 20, menu_y_start = 40;
    int menu_x_pad = 24, menu_y_pad = 18;

    nglInitQuad(&quad);
    nglSetQuadRect(&quad, menu_x_start, menu_y_start, menu_x_start + debug_width + menu_x_pad, menu_y_start + debug_height + menu_y_pad);
    nglSetQuadColor(&quad, 0xC8141414);
    nglSetQuadZ(&quad, 0.5f);
    nglListAddQuad(&quad);

    int white_color = nglColor(255, 255, 255, 255);
    int yellow_color = nglColor(255, 255, 0, 255);
    int green_color = nglColor(0, 255, 0, 255);
    int pink_color = nglColor(255, 0, 255, 255);

    int render_height = menu_y_start;
    render_height += 12;
    int render_x = menu_x_start;
    render_x += 8;
    nglListAddString(*nglSysFont, render_x, render_height, 0.2f, green_color, 1.f, 1.f, current_menu->title);
    render_height += getStringHeight(current_menu->title);

    if (current_menu->window_start) {
        nglListAddString(*nglSysFont, render_x, render_height, 0.2f, pink_color, 1.f, 1.f, UP_ARROW);
    }

    render_height += getStringHeight(UP_ARROW);

    for (int i = 0; i < total_elements_page; i++) {

        int current_color = current_menu->cur_index == i ? yellow_color : white_color;

        debug_menu_entry* entry = &current_menu->entries[current_menu->window_start + i];
        auto cur = getRealText(entry);
        nglListAddString(*nglSysFont, render_x, render_height, 0.2f, current_color, 1.f, 1.f, cur.c_str());
        render_height += getStringHeight(cur.c_str());
    }

    if (needs_down_arrow) {
        nglListAddString(*nglSysFont, render_x, render_height, 0.2f, pink_color, 1.f, 1.f, DOWN_ARROW);
        render_height += getStringHeight(DOWN_ARROW);
    }
}

typedef void (*nglListEndScene_ptr)();
nglListEndScene_ptr nglListEndScene = (nglListEndScene_ptr)0x00742B50;



void render_current_debug_menu2()
{
    auto UP_ARROW { " ^ ^ ^ " };
    auto DOWN_ARROW { " v v v " };

    int num_elements = min((DWORD)MAX_ELEMENTS_PAGE, current_menu->used_slots - current_menu->window_start);
    int needs_down_arrow = ((current_menu->window_start + MAX_ELEMENTS_PAGE) < current_menu->used_slots) ? 1 : 0;

    int cur_width, cur_height;
    int debug_width = 0;
    int debug_height = 0;

    auto get_and_update = [&](auto* x) {
        getStringDimensions(x, &cur_width, &cur_height);
        debug_height += cur_height;
        debug_width = max(debug_width, cur_width);
    };

    // printf("new size: %s %d %d (%d %d)\n", x, debug_width, debug_height, cur_width, cur_height);

    get_and_update(current_menu->title);
    get_and_update(UP_ARROW);

    int total_elements_page = needs_down_arrow ? MAX_ELEMENTS_PAGE : current_menu->used_slots - current_menu->window_start;

    for (int i = 0; i < total_elements_page; ++i) {
        debug_menu_entry* entry = &current_menu->entries[current_menu->window_start + i];
        auto cur = getRealText(entry);
        get_and_update(cur.c_str());
    }

    if (needs_down_arrow) {
        get_and_update(DOWN_ARROW);
    }

    nglQuad quad;

    int menu_x_start = 20, menu_y_start = 40;
    int menu_x_pad = 24, menu_y_pad = 18;

    nglInitQuad(&quad);
    nglSetQuadRect(&quad, menu_x_start, menu_y_start, menu_x_start + debug_width + menu_x_pad, menu_y_start + debug_height + menu_y_pad);
    nglSetQuadColor(&quad, 0x64141414);
    nglSetQuadZ(&quad, 0.5f);
    nglListAddQuad(&quad);

    int white_color = nglColor(255, 255, 255, 255);
    int yellow_color = nglColor(255, 255, 0, 255);
    int green_color = nglColor(0, 255, 0, 255);
    int pink_color = nglColor(255, 0, 255, 255);

    int render_height = menu_y_start;
    render_height += 12;
    int render_x = menu_x_start;
    render_x += 8;
    nglListAddString(*nglSysFont, render_x, render_height, 0.2f, green_color, 1.f, 1.f, current_menu->title);
    render_height += getStringHeight(current_menu->title);

    if (current_menu->window_start) {
        nglListAddString(*nglSysFont, render_x, render_height, 0.2f, pink_color, 1.f, 1.f, UP_ARROW);
    }

    render_height += getStringHeight(UP_ARROW);

    for (int i = 0; i < total_elements_page; i++) {

        int current_color = current_menu->cur_index == i ? yellow_color : white_color;

        debug_menu_entry* entry = &current_menu->entries[current_menu->window_start + i];
        auto cur = getRealText(entry);
        nglListAddString(*nglSysFont, render_x, render_height, 0.2f, current_color, 1.f, 1.f, cur.c_str());
        render_height += getStringHeight(cur.c_str());
    }

    if (needs_down_arrow) {
        nglListAddString(*nglSysFont, render_x, render_height, 0.2f, pink_color, 1.f, 1.f, DOWN_ARROW);
        render_height += getStringHeight(DOWN_ARROW);
    }
}

void myDebugMenu()
{
    if (debug_enabled) {
        render_current_debug_menu();
    }

    if (menu_disabled) {
        render_current_debug_menu2();
    }

    nglListEndScene();
}


typedef int (*wndHandler_ptr)(HWND, UINT, WPARAM, LPARAM);
wndHandler_ptr orig_WindowHandler = (wndHandler_ptr) 0x005941A0;

int WindowHandler(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam) {

	switch (Msg) {
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_KEYUP:
	case WM_KEYDOWN:
	case WM_INPUT:
		printf("swallowed keypress\n");
		return 0;
	}

	return orig_WindowHandler(hwnd, Msg, wParam, lParam);

}

/*
	STDMETHOD(GetDeviceState)(THIS_ DWORD,LPVOID) PURE;
	STDMETHOD(GetDeviceData)(THIS_ DWORD,LPDIDEVICEOBJECTDATA,LPDWORD,DWORD) PURE;

*/

typedef int (__stdcall* GetDeviceState_ptr)(IDirectInputDevice8*, DWORD, LPVOID);
GetDeviceState_ptr GetDeviceStateOriginal = nullptr;

typedef int(__fastcall* game_pause_unpause_ptr)(void*);
game_pause_unpause_ptr game_pause = (game_pause_unpause_ptr)0x0054FBE0;
game_pause_unpause_ptr game_unpause = (game_pause_unpause_ptr)0x0053A880;


typedef int (__fastcall* game_get_cur_state_ptr)(void* );
game_get_cur_state_ptr game_get_cur_state = (game_get_cur_state_ptr) 0x005363D0;

typedef int (__fastcall* world_dynamics_system_remove_player_ptr)(void* , void* edx, int number);
world_dynamics_system_remove_player_ptr world_dynamics_system_remove_player = (world_dynamics_system_remove_player_ptr) 0x00558550;

typedef int (__fastcall* world_dynamics_system_add_player_ptr)(void* , void* edx, mString* str);
world_dynamics_system_add_player_ptr world_dynamics_system_add_player = (world_dynamics_system_add_player_ptr) 0x0055B400;

typedef int (*entity_teleport_abs_po_ptr)(DWORD, float*, int one);
entity_teleport_abs_po_ptr entity_teleport_abs_po = (entity_teleport_abs_po_ptr) 0x004F3890;

typedef DWORD* (__fastcall* entity_variants_entity_variants_core_get_info_node_ptr)(DWORD* , void* edx, int a2, char a3);
entity_variants_entity_variants_core_get_info_node_ptr entity_variants_entity_variants_core_get_info_node = (entity_variants_entity_variants_core_get_info_node_ptr) 0x006A3390;

typedef void (*us_lighting_switch_time_of_day_ptr)(int time_of_day);
us_lighting_switch_time_of_day_ptr us_lighting_switch_time_of_day = (us_lighting_switch_time_of_day_ptr)0x00408790;




struct vm_executable;

typedef struct script_instance {
	uint8_t unk[0x2C];
	script_object* object;
        script_object* parent;
        script_object* get_parent(script_instance*)
        {
                return this->parent;
        }
} ;




struct vm_executable {
	struct {
        DWORD unk;
        script_executable *scriptexecutable;
    } *unk_struct;
	DWORD field_4;
	string_hash field_8;
	DWORD params;

};




typedef struct vm_thread {
        uint8_t unk[0xC];
        script_instance* instance;
        vm_executable* vmexecutable;
        const vm_executable* ex;

        const vm_executable* get_executable(vm_thread*) const
        {
        return this->ex;
        }
};

struct vm_stack {
	uint8_t unk[0x184];
	DWORD stack_ptr;
	vm_thread* thread;
        char* get_SP;
        char* SP;
        vm_thread* my_thread;


        void set_SP(char* sp)
        {
        SP = sp;
        }

        vm_thread* get_thread()
        {
        return my_thread;
        }


    void push(void* data, DWORD size) {
        memcpy((void *)this->stack_ptr, data, size);
        this->stack_ptr += size;
    }

    void pop(DWORD size) {
        this->stack_ptr -= size;
    }

};


uint8_t __stdcall slf__debug_menu_entry__set_handler__str(vm_stack *stack, void* unk) {

	stack->pop(8);

	void** params = (void**)stack->stack_ptr;

	debug_menu_entry* entry = static_cast<decltype(entry)>(params[0]);
	char* scrpttext = static_cast<char *>(params[1]);

	string_hash strhash {scrpttext};

	script_instance* instance = stack->thread->instance;
	int functionid = instance->object->find_func(strhash);
	entry->data = instance;
	entry->data1 = (void *) functionid;
	
	return 1;
}

uint8_t __stdcall slf__destroy_debug_menu_entry__debug_menu_entry(vm_stack* function, void* unk) {

	function->pop(4);

	debug_menu_entry** entry = (decltype(entry)) function->stack_ptr;

	remove_debug_menu_entry(*entry);

	return 1;
}

void handle_progression_select_entry(debug_menu_entry* entry);

void handle_script_select_entry(debug_menu_entry* entry)
{
        handle_progression_select_entry(entry);
}



void sub_65BB36(script_library_class::function *func, vm_stack *stack, char *a3, int a4)
{
    for ( auto i = 0; i < a4; ++i )
    {
        if ( *bit_cast<DWORD *>(&a3[4 * i]) == 0x7BAD05CF )
        {
            printf("uninitialized parameters in call to 0x%08X", func->m_vtbl);

            //v5 = j_vm_stack::get_thread(stack);
            //vm_thread::slf_error(v5, v6);

            assert(0 && "uninitialized parameters in call to script library function");
        }
    }
}

uint8_t __fastcall slf__create_progression_menu_entry(script_library_class::function *func, void *, vm_stack *stack, void *unk) {

	stack->pop(8);

    auto *stack_ptr = bit_cast<char *>(stack->stack_ptr);
    sub_65BB36(func, stack, stack_ptr, 2);

	char** strs = (char **)stack->stack_ptr;

	printf("Entry: %s -> %s\n", strs[0], strs[1]);


	string_hash strhash {strs[1]};

	script_instance* instance = stack->thread->instance;
	int functionid = instance->object->find_func(strhash);

	debug_menu_entry entry {};
	entry.entry_type = UNDEFINED;
	entry.data = instance;
	entry.data1 = (void *) functionid;

	strcpy(entry.text, strs[0]);
    entry.set_game_flags_handler(handle_progression_select_entry);

	add_debug_menu_entry(progression_menu, &entry);

	/*
	if(function->thread->instance->object->vmexecutable[functionid]->params != 4)
	*/
	
	int push = 0;
	stack->push(&push, sizeof(push));
	return true;
}




bool __fastcall slf__create_debug_menu_entry(script_library_class::function *func, void *, vm_stack* stack, void* unk) {
	stack->pop(4);

    auto *stack_ptr = bit_cast<char *>(stack->stack_ptr);
    sub_65BB36(func, stack, stack_ptr, 2);
	char** strs = bit_cast<char **>(stack->stack_ptr);

	//printf("Entry: %s ", strs[0]);

	debug_menu_entry entry {};
	entry.entry_type = UNDEFINED;
	strcpy(entry.text, strs[0]);
    entry.set_game_flags_handler(handle_script_select_entry);

    printf("entry.text = %s\n", entry.text);

	script_instance* instance = stack->thread->instance;
    printf("Total funcs: %d\n", instance->object->total_funcs);

	void *res = add_debug_menu_entry(script_menu, &entry);

	script_executable* se = stack->thread->vmexecutable->unk_struct->scriptexecutable;
    printf("total_script_objects = %d\n", se->total_script_objects);
    for (auto i = 0; i < se->total_script_objects; ++i) {
        auto *so = se->field_28[i];
        printf("Name of script_object = %s\n", so->field_0.to_string());

        for (auto i = 0; i < so->total_funcs; ++i) {
            printf("Func name: %s\n", so->funcs[i]->field_8.to_string());
        }

        printf("\n");
    }

    script_executable* v7;
    vm_stack* v3 = stack;
    vm_thread* v4;
    vm_executable* v5;
    script_object* owner;
    script_instance* v2;
	se->add_allocated_stuff(vm_debug_menu_entry_garbage_collection_id, (int) res, 0);
     v3->get_thread();
        v4->get_executable(v4);
     v2->get_parent(v2);
        owner->get_owner(v5);

	//printf("%08X\n", res);

	int push = (int) res;
	stack->push(&push, sizeof(push));
	return 1;
}

DWORD modulo(int num, DWORD mod) {
	if (num >= 0) {
		return num % mod;
	}

	int absolute = abs(num);
	if (absolute % mod == 0)
		return 0;

	return mod - absolute % mod;
}


void menu_go_down() {


	if ((current_menu->window_start + MAX_ELEMENTS_PAGE) < current_menu->used_slots) {

		if (current_menu->cur_index < MAX_ELEMENTS_PAGE / 2)
			++current_menu->cur_index;
		else
			++current_menu->window_start;
	}
	else {

		int num_elements = min((DWORD) MAX_ELEMENTS_PAGE, current_menu->used_slots - current_menu->window_start);
		current_menu->cur_index = modulo(current_menu->cur_index + 1, num_elements);
		if (current_menu->cur_index == 0)
			current_menu->window_start = 0;
	}
}

void menu_go_up() {

	int num_elements = min( (DWORD) MAX_ELEMENTS_PAGE, current_menu->used_slots - current_menu->window_start);
	if (current_menu->window_start) {


		if (current_menu->cur_index > MAX_ELEMENTS_PAGE / 2)
			current_menu->cur_index--;
		else
			current_menu->window_start--;

	}
	else {

		int num_elements = min(MAX_ELEMENTS_PAGE, current_menu->used_slots - current_menu->window_start);
		current_menu->cur_index = modulo(current_menu->cur_index - 1, num_elements);
		if (current_menu->cur_index == (num_elements - 1))
			current_menu->window_start = current_menu->used_slots - num_elements;

	}

}

typedef enum {
	MENU_TOGGLE,
	MENU_ACCEPT,
	MENU_BACK,
    MENU_START,
    MENU_SELECT,

	MENU_UP,
	MENU_DOWN,
	MENU_LEFT,
	MENU_RIGHT,


	MENU_KEY_MAX
} MenuKey;

uint32_t controllerKeys[MENU_KEY_MAX];

int get_menu_key_value(MenuKey key,  int keyboard)
{
    if (keyboard) {

        int i = 0;
        switch (key) {
        case MENU_TOGGLE:
            i = DIK_INSERT;
            break;
        case MENU_ACCEPT:
            i = DIK_RETURN;
            break;
        case MENU_BACK:
            i = DIK_ESCAPE;
            break;
        case MENU_START:
            i = DIK_S;
            break;
        case MENU_SELECT:
            i = DIK_D;
            break;

        case MENU_UP:
            i = DIK_UPARROW;
            break;
        case MENU_DOWN:
            i = DIK_DOWNARROW;
            break;
        case MENU_LEFT:
            i = DIK_LEFTARROW;
            break;
        case MENU_RIGHT:
            i = DIK_RIGHTARROW;
            break;
        }
        return keys[i];
    }

    return controllerKeys[key];
}
int get_menu_key_value2(MenuKey key, int keyboard, MenuKey key2, int keyboard2)
{
    if (keyboard) {

        int i = 0;
        switch (key) {
        case MENU_TOGGLE:
            i = DIK_INSERT;
            break;
        case MENU_ACCEPT:
            i = DIK_RETURN;
            break;
        case MENU_BACK:
            i = DIK_ESCAPE;
            break;
        case MENU_START:
            i = DIK_S;
            break;
        case MENU_SELECT:
            i = DIK_D;
            break;

        case MENU_UP:
            i = DIK_UPARROW;
            break;
        case MENU_DOWN:
            i = DIK_DOWNARROW;
            break;
        case MENU_LEFT:
            i = DIK_LEFTARROW;
            break;
        case MENU_RIGHT:
            i = DIK_RIGHTARROW;
            break;
        }
        return keys[i];
    }

    return controllerKeys[key];
}





int is_menu_key_pressed(MenuKey key, int keyboard) {
	return (get_menu_key_value(key, keyboard) == 2);
}

int is_menu_key_pressed2(MenuKey key,int keyboard, MenuKey key2, int keyboard2)
{
    return (get_menu_key_value2(key, keyboard, key2, keyboard2) == 2);
}

int is_menu_key_clicked(MenuKey key, int keyboard) {
	return get_menu_key_value(key, keyboard);
}

void GetDeviceStateHandleKeyboardInput(LPVOID lpvData) {
	BYTE* keysCurrent = (BYTE *) lpvData;

	for (int i = 0; i < 256; i++) {

		if (keysCurrent[i]) {
			keys[i]++;
		}
		else {
			keys[i] = 0;
		}
	}

	
}

void read_and_update_controller_key_button(LPDIJOYSTATE2 joy, int index, MenuKey key) {
	int res = 0;
	if (joy->rgbButtons[index]) {
		++controllerKeys[key];
	}
	else {
		controllerKeys[key] = 0;
	}
}


void read_and_update_controller_key_dpad(LPDIJOYSTATE2 joy, int angle, MenuKey key) {
	
	if (joy->rgdwPOV[0] == 0xFFFFFFFF)
		controllerKeys[key] = 0;
	else
		controllerKeys[key] = (joy->rgdwPOV[0] == angle) ? controllerKeys[key] + 1 : 0;
}


void GetDeviceStateHandleControllerInput(LPVOID lpvData) {
	LPDIJOYSTATE2 joy = (decltype(joy)) lpvData;

	read_and_update_controller_key_button(joy, 1, MENU_ACCEPT);
	read_and_update_controller_key_button(joy, 2, MENU_BACK);
	read_and_update_controller_key_button(joy, 12, MENU_TOGGLE);
    read_and_update_controller_key_button(joy, 8, MENU_START);
    read_and_update_controller_key_button(joy, 9, MENU_SELECT);

	read_and_update_controller_key_dpad(joy, 0, MENU_UP);
	read_and_update_controller_key_dpad(joy, 9000, MENU_RIGHT);
	read_and_update_controller_key_dpad(joy, 18000, MENU_DOWN);
	read_and_update_controller_key_dpad(joy, 27000, MENU_LEFT);
}

typedef int (*resource_manager_can_reload_amalgapak_ptr)(void);
resource_manager_can_reload_amalgapak_ptr resource_manager_can_reload_amalgapak = (resource_manager_can_reload_amalgapak_ptr) 0x0053DE90;

typedef void (*resource_manager_reload_amalgapak_ptr)(void);
resource_manager_reload_amalgapak_ptr resource_manager_reload_amalgapak = (resource_manager_reload_amalgapak_ptr) 0x0054C2E0;





struct mission_t
{
    std::string field_0;
    const char *field_C;
    int field_10;
    int field_14;
};

std::vector<mission_t> menu_missions; 
 

void mission_unload_handler(debug_menu_entry *a1)
{
    auto *v1 = mission_manager::s_inst();
    v1->prepare_unload_script();

	close_debug();
}





void mission_select_handler(debug_menu_entry* entry)
{
        auto v1 = entry->m_id;
        auto& v7 = menu_missions[v1];
        auto v6 = v7.field_C;
        auto v5 = v7.field_14;
        auto* v4 = v7.field_0.c_str();
        auto v3 = v7.field_10;
        auto* v2 = mission_manager::s_inst();
        v2->force_mission2(v3, v4, v5, v6);
        debug_menu::hide();
}

void create_game_menu(debug_menu* parent);


void _populate_missions()
{
        auto handle_table = [](mission_table_container* table, int district_id) -> void {
            std::vector<mission_table_container::script_info> script_infos {};

            if (table != nullptr) {
                table->append_script_info(&script_infos);
            }

            for (auto& info : script_infos) {
                mString a2 { "pk_" };
                auto v19 = a2 + info.field_0;
                auto* v11 = v19.c_str();
                auto key = create_resource_key_from_path(v11, RESOURCE_KEY_TYPE_PACK);
                if (resource_manager::get_pack_file_stats(key, nullptr, nullptr, nullptr)) {
                    mission_t mission {};
                    mission.field_0 = info.field_0;
                    mission.field_10 = district_id;
                    mission.field_14 = info.field_8;

                    mission.field_C = info.field_4->get_script_data_name();
                    menu_missions.push_back(mission);

                    auto v47 = [](mission_t& mission) -> mString {
                        if (mission.field_C != nullptr) {
                            auto* v17 = mission.field_C;
                            auto* v14 = mission.field_0.c_str();
                            mString str { 0, "%s (%s)", v14, v17 };
                            return str;
                        }

                        auto v18 = mission.field_14;
                        auto* v15 = mission.field_0.c_str();
                        mString str { 0, "%s (%d)", v15, v18 };
                        return str;
                    }(mission);

                    printf(v47.c_str());
                }
            }
        };

        auto* v2 = mission_manager::s_inst();
        auto count = v2->get_district_table_count();
        printf("%s %d", "table_count = ", count);

        {
                auto* v3 = mission_manager::s_inst();
                auto* table = v3->get_global_table();

                handle_table(table, 0);
        }

        std::for_each(&v2->m_district_table_containers[0], &v2->m_district_table_containers[0] + count, [&handle_table](auto* table) {
            auto* reg = table->get_region();
            auto& v6 = reg->get_name();
            fixedstring32 v53 { v6.to_string() };

            auto district_id = reg->get_district_id();

            // sp_log("%d %s", i, v53.to_string());

            handle_table(table, district_id);
        });

        assert(0);
}

void populate_missions_menu(debug_menu_entry* entry)
{
                                                        menu_missions = {};
                        if (resource_manager::can_reload_amalgapak()) {
                            resource_manager::load_amalgapak();
                        }

        auto* head_menu = create_menu("Missions", debug_menu::sort_mode_t::ascending);
        entry->set_submenu(head_menu);

        auto* mission_unload_entry = create_menu_entry(mString { "UNLOAD CURRENT MISSION" });

        mission_unload_entry->set_game_flags_handler(mission_unload_handler);
        head_menu->add_entry(mission_unload_entry);

        auto* v2 = mission_manager::s_inst();
        auto v58 = v2->get_district_table_count();
        for (auto i = -1; i < v58; ++i) {
                fixedstring32 v53 {};
                int v52;
                mission_table_container* table = nullptr;
                if (i == -1) {
                        table = v2->get_global_table();
                        fixedstring32 a1 { "global" };
                        v53 = a1;
                        v52 = 0;
                } else {
                        table = v2->get_district_table(i);
                        auto* reg = table->get_region();
                        auto& v6 = reg->get_name();
                        v53 = v6;

                        auto v52 = reg->get_district_id();


 
        auto* v25 = create_menu(v53.to_string(), debug_menu::sort_mode_t::ascending);

                         debug_menu_entry v26 { v25 };
                        add_debug_menu_entry(head_menu,&v26);

        auto* v28 = create_menu("", (menu_handler_function)empty_handler, 10);
                        debug_menu_entry v30 { v28 };
        v30.entry_type = NORMAL;
                        debug_menu_entry v29 { v30 };
                        add_debug_menu_entry(v25, &v29);
                }

                std::vector<mission_table_container::script_info> script_infos;

                if (table != nullptr) {
                        auto res = table->append_script_info(&script_infos);
                        assert(res);
                }

                for (auto& info : script_infos) {


                        auto v50 = menu_missions.size();
                        mString a2 { "pk_" };
                        auto v19 = a2 + info.field_0;
                        auto* v11 = v19.c_str();
                        auto key = create_resource_key_from_path(v11, RESOURCE_KEY_TYPE_PACK);
                        if (resource_manager::get_pack_file_stats(key, nullptr, nullptr, nullptr)) {
                            mission_t mission {};
                            mission.field_0 = info.field_0;
                            mission.field_10 = v52;
                            mission.field_14 = info.field_8;

                            mission.field_C = info.field_4->get_script_data_name();
                            menu_missions.push_back(mission);

                            mString v47 {};
                            if (mission.field_C != nullptr) {
                                auto* v17 = mission.field_C;
                                auto* v14 = mission.field_0.c_str();
                                auto a2a = mString { 0, "%s (%s)", v14, v17 };
                                v47 = a2a;
                            } else {
                                auto v18 = mission.field_14;
                                auto* v15 = mission.field_0.c_str();
                                auto a2b = mString { 0, "%s (%d)", v15, v18 };
                                v47 = a2b;
                            }
                            auto* v27 = create_menu_entry(v47);

                            auto* v43 = v27;
                            auto* v46 = v27;
                            v27->set_id(v50);
                            v46->set_game_flags_handler(mission_select_handler);
                            head_menu->add_entry(v46);

                            auto handle_table = [](mission_table_container* table, int district_id) -> void {
                                std::vector<mission_table_container::script_info> script_infos {};

                                if (table != nullptr) {
                                    table->append_script_info(&script_infos);
                                }

                                for (auto& info : script_infos) {
                                    mString a2 { "pk_" };
                                    auto v19 = a2 + info.field_0;
                                    auto* v11 = v19.c_str();
                                    auto key = create_resource_key_from_path(v11, 25);
                                    if (resource_manager::get_pack_file_stats(key, nullptr, nullptr, nullptr)) {
                                        mission_t mission {};
                                        mission.field_0 = info.field_0;
                                        mission.field_10 = district_id;
                                        mission.field_14 = info.field_8;

                                        mission.field_C = info.field_4->get_script_data_name();
                                        menu_missions.push_back(mission);

                                        auto v47 = [](mission_t& mission) -> mString {
                                            if (mission.field_C != nullptr) {
                                                auto* v17 = mission.field_C;
                                                auto* v14 = mission.field_0.c_str();
                                                mString str { 0, "%s (%s)", v14, v17 };
                                                return str;
                                            }

                                            auto v18 = mission.field_14;
                                            auto* v15 = mission.field_0.c_str();
                                            mString str { 0, "%s (%d)", v15, v18 };
                                            return str;
                                        }(mission);

                                        printf(v47.c_str());
                                    }
                                }
                            };

                            auto* v2 = mission_manager::s_inst();
                            auto count = v2->get_district_table_count();
                            printf("%s %d", "table_count = ", count);

                            {
                                auto* v3 = mission_manager::s_inst();
                                auto* table = v3->get_global_table();

                                handle_table(table, 0);
                            }

                            std::for_each(&v2->m_district_table_containers[0], &v2->m_district_table_containers[0] + count, [&handle_table](auto* table) {
                                auto* reg = table->get_region();
                                auto& v6 = reg->get_name();
                                fixedstring32 v53 { v6.to_string() };

                                auto district_id = reg->get_district_id();

                                // sp_log("%d %s", i, v53.to_string());

                                handle_table(table, district_id);
                            });

                            assert(0);
                        }
                }
        }
}



void create_missions_menu(debug_menu* parent)
{
        auto* missions_menu = create_menu("Missions", debug_menu::sort_mode_t::undefined);
        auto* v2 = create_menu_entry(missions_menu);
        v2->set_game_flags_handler(populate_missions_menu);
        parent->add_entry(v2);
}


void disable_physics()
{
        debug_enabled = 1;
        game_unpause(g_game_ptr());
        current_menu = current_menu;
        g_game_ptr()->enable_physics(false);
}

void enable_physics()
{
        menu_disabled = 1;
        game_unpause(g_game_ptr());
        current_menu = current_menu;
        g_game_ptr()->enable_physics(true);
}

void custom()
{
        menu_disabled = 1;
        game_unpause(g_game_ptr());
        current_menu = current_menu;
        g_game_ptr()->enable_physics(false);
}





void menu_setup(int game_state, int keyboard, int keyboard2)
{

        // debug menu stuff
    if (is_menu_key_pressed(MENU_TOGGLE, keyboard) && (game_state == 6 || game_state == 7)) {

        if (!debug_enabled && game_state == 6) {
            game_unpause(g_game_ptr());
            debug_enabled = !debug_enabled;
            current_menu = debug_menu::root_menu;
            custom();



        }
            
       

        else if (!menu_disabled && game_state == 6) {
            game_unpause(g_game_ptr());
            menu_disabled = !menu_disabled;
            current_menu = current_menu;
            disable_physics();

        }

        else if (!debug_enabled && game_state == 6) {
            game_unpause(g_game_ptr());
            debug_enabled = !debug_enabled;
            current_menu = current_menu;
            disable_physics();

        }

        else if (!debug_enabled, menu_disabled && game_state == 6) {
            game_unpause(g_game_ptr());
            menu_disabled, debug_enabled = !menu_disabled, debug_enabled;
            current_menu = current_menu;
            enable_physics();
        }
    }

     if (is_menu_key_pressed(MENU_TOGGLE, keyboard) && (game_state == 6 || game_state == 7)) {


        if (!debug_enabled && game_state == 7) {
            game_unpause(g_game_ptr());
            debug_enabled = !debug_enabled;
            current_menu = debug_menu::root_menu;
            disable_physics();

        }

        else if (!menu_disabled && game_state == 7) {
            game_unpause(g_game_ptr());
            menu_disabled = !menu_disabled;
            current_menu = current_menu;
            disable_physics();

        }

        else if (!debug_enabled && game_state == 7) {
            game_unpause(g_game_ptr());
            debug_enabled = !debug_enabled;
            current_menu = current_menu;
            disable_physics();

        }

        else if (!debug_enabled, menu_disabled && game_state == 7) {
            game_unpause(g_game_ptr());
            menu_disabled, debug_enabled = !menu_disabled, debug_enabled;
            current_menu = current_menu;
            enable_physics();
        }

        if (!debug_enabled && game_state == 7) {
            game_unpause(g_game_ptr());
            debug_enabled = !debug_enabled;
            current_menu = debug_menu::root_menu;
            disable_physics();

        }

        else if (!menu_disabled && game_state == 7) {
            game_unpause(g_game_ptr());
            menu_disabled = !menu_disabled;
            current_menu = current_menu;
            disable_physics();

        }

        else if (!debug_enabled && game_state == 7) {
            game_unpause(g_game_ptr());
            debug_enabled = !debug_enabled;
            current_menu = current_menu;
            disable_physics();

        }

        else if (!debug_enabled, menu_disabled && game_state == 7) {
            game_unpause(g_game_ptr());
            menu_disabled, debug_enabled = !menu_disabled, debug_enabled;
            current_menu = current_menu;
            enable_physics();
        }



        _populate_missions();
            



        
    }
}



void menu_input_handler(int keyboard, int SCROLL_SPEED) {
	if (is_menu_key_clicked(MENU_DOWN, keyboard)) {

		int key_val = get_menu_key_value(MENU_DOWN, keyboard);
		if (key_val == 1) {
			menu_go_down();
		}
		else if ((key_val >= SCROLL_SPEED) && (key_val % SCROLL_SPEED == 0)) {
			menu_go_down();
		}
	}
	else if (is_menu_key_clicked(MENU_UP, keyboard)) {

		int key_val = get_menu_key_value(MENU_UP, keyboard);
		if (key_val == 1) {
			menu_go_up();
		}
		else if ((key_val >= SCROLL_SPEED) && (key_val % SCROLL_SPEED == 0)) {
			menu_go_up();
		}
	}
	else if (is_menu_key_pressed(MENU_ACCEPT, keyboard))
    {
        auto *entry = &current_menu->entries[current_menu->window_start + current_menu->cur_index];
        assert(entry != nullptr);
        entry->on_select(1.0);

		//current_menu->handler(entry, ENTER);
	}
	else if (is_menu_key_pressed(MENU_BACK, keyboard)) {
		current_menu->go_back();
	}

 	else if (is_menu_key_pressed(MENU_LEFT, keyboard) || is_menu_key_pressed(MENU_RIGHT, keyboard)) {

                debug_menu_entry* cur = &current_menu->entries[current_menu->window_start + current_menu->cur_index];

                if (cur->entry_type == POINTER_BOOL || cur->entry_type == POINTER_NUM || cur->entry_type == CAMERA_FLOAT || cur->entry_type == POINTER_INT || cur->entry_type == POINTER_FLOAT || cur->entry_type == FLOAT_E || cur->entry_type == INTEGER2 || cur->entry_type == INTEGER) {
       custom_key_type pressed = (is_menu_key_pressed(MENU_LEFT, keyboard) ? LEFT : RIGHT);
                        if (is_menu_key_pressed(MENU_LEFT, keyboard)) {
                            cur->on_change(-1.0, false);
                        } else {
                            cur->on_change(1.0, true);
                        }
                
        
        switch (cur->entry_type) {
        case BOOLEAN_E:
            current_menu->handler(cur, pressed);
            break;
        case INTEGER:
            if (cur->custom_handler != NULL) {
                cur->custom_handler(cur, pressed, current_menu->handler);
            } else {
                current_menu->handler(cur, pressed);
            }
        }
        
}
        }


	debug_menu_entry *highlighted = &current_menu->entries[current_menu->window_start + current_menu->cur_index];
    assert(highlighted->frame_advance_callback != nullptr);
    highlighted->frame_advance_callback(highlighted);
}


HRESULT __stdcall GetDeviceStateHook(IDirectInputDevice8* self, DWORD cbData, LPVOID lpvData) {

	HRESULT res = GetDeviceStateOriginal(self, cbData, lpvData);

	//printf("cbData %d %d %d\n", cbData, sizeof(DIJOYSTATE), sizeof(DIJOYSTATE2));


	
	//keyboard time babyyy
	if (cbData == 256 || cbData == sizeof(DIJOYSTATE2)) {

		
		if (cbData == 256)
			GetDeviceStateHandleKeyboardInput(lpvData);
		else if (cbData == sizeof(DIJOYSTATE2))
			GetDeviceStateHandleControllerInput(lpvData);

		int game_state = 0;
		if (g_game_ptr())
        {
			game_state = game_get_cur_state(g_game_ptr());
        }

		//printf("INSERT %d %d %c\n", keys[DIK_INSERT], game_state, debug_enabled ? 'y' : 'n');

		int keyboard = cbData == 256;
        int keyboard2 = cbData == 256;
        menu_setup(game_state, keyboard, keyboard2);


		if (debug_enabled) {
			menu_input_handler(keyboard, 5);
		}

	}


	if (debug_enabled) {
		memset(lpvData, 0, cbData);
	}



	//printf("Device State called %08X %d\n", this, cbData);

	return res;
}

typedef HRESULT(__stdcall* GetDeviceData_ptr)(IDirectInputDevice8*, DWORD, LPDIDEVICEOBJECTDATA, LPDWORD, DWORD);
GetDeviceData_ptr GetDeviceDataOriginal = nullptr;

HRESULT __stdcall GetDeviceDataHook(IDirectInputDevice8* self, DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD dwFlags) {

	HRESULT res = GetDeviceDataOriginal(self, cbObjectData, rgdod, pdwInOut, dwFlags);

	printf("data\n");
	if (res == DI_OK) {

		printf("All gud\n");
		for (int i = 0; i < *pdwInOut; i++) {


			if (LOBYTE(rgdod[i].dwData) > 0) {

				if (rgdod[i].dwOfs == DIK_ESCAPE) {

					printf("Pressed escaped\n");
					__debugbreak();
				}
			}
		}
	}
	//printf("Device Data called %08X\n", this);

	return res;
}



typedef HRESULT(__stdcall* IDirectInput8CreateDevice_ptr)(IDirectInput8W*, const GUID*, LPDIRECTINPUTDEVICE8W*, LPUNKNOWN);
IDirectInput8CreateDevice_ptr createDeviceOriginal = nullptr;

HRESULT  __stdcall IDirectInput8CreateDeviceHook(IDirectInput8W* self, const GUID* guid, LPDIRECTINPUTDEVICE8W* device, LPUNKNOWN unk) {

	//printf("CreateDevice %d %d %d %d %d %d %d\n", *guid, GUID_SysMouse, GUID_SysKeyboard, GUID_SysKeyboardEm, GUID_SysKeyboardEm2, GUID_SysMouseEm, GUID_SysMouseEm2);
	printf("Guid = {%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}\n",
		guid->Data1, guid->Data2, guid->Data3,
		guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
		guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);

	HRESULT res = createDeviceOriginal(self, guid, device, unk);


	if (IsEqualGUID(GUID_SysMouse, *guid))
		return res; // ignore mouse

	if (IsEqualGUID(GUID_SysKeyboard, *guid))
		puts("Found the keyboard");
	else
		puts("Hooking something different...maybe a controller");

#if 0
	DWORD* vtbl = (*device)->lpVtbl;
	if (!GetDeviceStateOriginal) {
		GetDeviceStateOriginal = vtbl[9];
		vtbl[9] = GetDeviceStateHook;
	}

	if (!GetDeviceDataOriginal) {
		GetDeviceDataOriginal = vtbl[10];
		vtbl[10] = GetDeviceDataHook;
	}
#else
    if (GetDeviceStateOriginal == nullptr) {
        GetDeviceStateOriginal = (GetDeviceState_ptr)
            HookVTableFunction((void *) *device, (void *) GetDeviceStateHook, 9);
    }

    if (GetDeviceDataOriginal == nullptr) {
        GetDeviceDataOriginal = (GetDeviceData_ptr) HookVTableFunction((void *) *device,
                                                                       (void *) GetDeviceDataHook,
                                                                       10);
    }
#endif

	return res;
}

typedef HRESULT(__stdcall* IDirectInput8Release_ptr)(IDirectInput8W*);
IDirectInput8Release_ptr releaseDeviceOriginal = nullptr;

HRESULT  __stdcall IDirectInput8ReleaseHook(IDirectInput8W* self) {

	printf("Release\n");

	return releaseDeviceOriginal(self);
}

/*
BOOL CALLBACK EnumDevices(LPCDIDEVICEINSTANCE lpddi, LPVOID buffer) {

	wchar_t wGUID[40] = { 0 };
	char cGUID[40] = { 0 };

	//printf("%d\n", lpddi->guidProduct);
}
*/

typedef HRESULT(__stdcall* DirectInput8Create_ptr)(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID* ppvOut, LPUNKNOWN punkOuter);
HRESULT __stdcall HookDirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID* ppvOut, LPUNKNOWN punkOuter)
{
	DirectInput8Create_ptr caller = (decltype(caller)) *(void**)0x00987944;
	HRESULT res = caller(hinst, dwVersion, riidltf, ppvOut, punkOuter);

	IDirectInput8* iDir = (IDirectInput8 *) (*ppvOut);
	//printf("it's me mario %08X %08X\n", ppvOut, (*iDir)->lpVtbl);

#if 0
	DWORD* vtbl = (DWORD*)(*iDir)->lpVtbl;
	if (!createDeviceOriginal) {
		createDeviceOriginal = vtbl[3];
		vtbl[3] = IDirectInput8CreateDeviceHook;
	}
#else
    if (createDeviceOriginal == nullptr) {
        createDeviceOriginal = (IDirectInput8CreateDevice_ptr)
            HookVTableFunction((void *) iDir, (void *) IDirectInput8CreateDeviceHook, 3);
    }
#endif

	//(*iDir)->lpVtbl->EnumDevices(*iDir, DI8DEVCLASS_ALL, EnumDevices, NULL, DIEDFL_ALLDEVICES);
	return res;
}

DWORD hookDirectInputAddress = (DWORD) HookDirectInput8Create;

/*
void update_state() {

	while (1) {
		OutputDebugStringA("PILA %d", 6);
	}
}
*/


       typedef int(__fastcall* PauseMenuSystem_ptr)(font_index a2);
PauseMenuSystem_ptr PauseMenuSystem = (PauseMenuSystem_ptr)0x00647E50;

typedef int(__fastcall* game_handle_game_states_ptr)(game* , void* edx, void* a2);
game_handle_game_states_ptr game_handle_game_states_original = (game_handle_game_states_ptr) 0x0055D510;

int __fastcall game_handle_game_states(game* self, void* edx, void* a2) {

	if (!g_game_ptr()) {
		g_game_ptr() = self;
	}

	/*
	if (game_get_cur_state(this) == 14)
		__debugbreak();
		*/


		//printf("Current state %d %08X\n", game_get_cur_state(this), g_game_ptr);

	return game_handle_game_states_original(self, edx, a2);
}


typedef DWORD(__fastcall* entity_variants_hero_base_state_check_transition_ptr)(DWORD* , void* edx, DWORD* a2, int a3);
entity_variants_hero_base_state_check_transition_ptr entity_variants_hero_base_state_check_transition = (entity_variants_hero_base_state_check_transition_ptr) 0x00478D80;

DWORD __fastcall entity_variants_hero_base_state_check_transition_hook(DWORD* self, void* edx, DWORD* a2, int a3) {
	entity_variants_current_player = self;
	return entity_variants_hero_base_state_check_transition(self, edx, a2, a3);
}

typedef DWORD* (__fastcall* get_info_node_ptr)(void*, void* edx, int a2, char a3);
get_info_node_ptr get_info_node = (get_info_node_ptr) 0x006A3390;

DWORD* __fastcall get_info_node_hook(void* self, void* edx, int a2, char a3) {

	DWORD* res = get_info_node(self, edx, a2, a3);

	fancy_player_ptr = res;
	return res;
}

void init_shadow_targets()
{
    debug_menu::init();

    CDECL_CALL(0x00592E80);
}


typedef void (__fastcall* resource_pack_streamer_load_internal_ptr)(void* self, void* edx, char* str, int a3, int a4, int a5);
resource_pack_streamer_load_internal_ptr resource_pack_streamer_load_internal = (resource_pack_streamer_load_internal_ptr) 0x0054C580;

void __fastcall resource_pack_streamer_load_internal_hook(void* self, void* edx, char* str, int a3, int a4, int a5) {

	resource_pack_streamer_load_internal(self, edx, str, a3, a4, a5);
}

uint8_t __fastcall os_developer_options(BYTE *self, void *edx, int flag) {

	char** flag_list = (decltype(flag_list)) 0x936420;
	char* flag_text = flag_list[flag];
		
	uint8_t res = self[flag + 4];

	if (flag == 0x90) {
		printf("Game wants to know about: %d (%s) -> %d\n", flag, flag_text, res);
		__debugbreak();
	}
	
	
	//this[5 + 4] = 1;
	
	return res;
}

unsigned int hook_controlfp(unsigned int, unsigned int) {
    return {};
}

static constexpr uint32_t NOP = 0x90;

void set_nop(ptrdiff_t address, size_t num_bytes) {
    for (size_t i = 0u; i < num_bytes; ++i) {
        *bit_cast<uint8_t *>(static_cast<size_t>(address) + i) = NOP;
    }
}

struct FEMenuSystem;









void install_patches() {


    //fix invalid float operation
    {
        set_nop(0x005AC34C, 6);

        HookFunc(0x005AC347, (DWORD) hook_controlfp, 0, "Patching call to controlfp");
    }

    REDIRECT(0x005E10EE, init_shadow_targets);

    ngl_patch();


    game_patch();

        wds_patch();

    level_patch();


    mission_manager_patch();

    slab_allocator_patch();

    HookFunc(0x0052B4BF, (DWORD) spider_monkey::render, 0, "Patching call to spider_monkey::render");

	HookFunc(0x004EACF0, (DWORD)aeps_RenderAll, 0, "Patching call to aeps::RenderAll");

	HookFunc(0x0052B5D7, (DWORD)myDebugMenu, 0, "Hooking nglListEndScene to inject debug menu");
	//save orig ptr
	nflSystemOpenFile_orig = *nflSystemOpenFile_data;
	*nflSystemOpenFile_data = &nflSystemOpenFile;
	printf("Replaced nflSystemOpenFile %08X -> %08X\n", (DWORD)nflSystemOpenFile_orig, (DWORD)&nflSystemOpenFile);


	ReadOrWrite_orig = *ReadOrWrite_data;
	*ReadOrWrite_data = &ReadOrWrite;
	printf("Replaced ReadOrWrite %08X -> %08X\n", (DWORD)ReadOrWrite_orig, (DWORD)&ReadOrWrite);

	*(DWORD*)0x008218B2 = (DWORD) &hookDirectInputAddress;
	printf("Patching the DirectInput8Create call\n");


	HookFunc(0x0055D742, (DWORD)game_handle_game_states, 0, "Hooking handle_game_states");

	/*
	WriteDWORD(0x00877524, entity_variants_hero_base_state_check_transition_hook, "Hooking check_transition for peter hooded");
	WriteDWORD(0x00877560, entity_variants_hero_base_state_check_transition_hook, "Hooking check_transition for spider-man");
	WriteDWORD(0x0087759C, entity_variants_hero_base_state_check_transition_hook, "Hooking check_transition for venom");
	*/

	HookFunc(0x00478DBF, (DWORD) get_info_node_hook, 0, "Hook get_info_node to get player ptr");


	WriteDWORD(0x0089C710, (DWORD) slf__create_progression_menu_entry, "Hooking first ocurrence of create_progession_menu_entry");
	WriteDWORD(0x0089C718, (DWORD) slf__create_progression_menu_entry, "Hooking second  ocurrence of create_progession_menu_entry");


	WriteDWORD(0x0089AF70, (DWORD) slf__create_debug_menu_entry, "Hooking first ocurrence of create_debug_menu_entry");
	WriteDWORD(0x0089C708, (DWORD) slf__create_debug_menu_entry, "Hooking second  ocurrence of create_debug_menu_entry");


	HookFunc(0x005AD77D, (DWORD) construct_client_script_libs_hook, 0, "Hooking construct_client_script_libs to inject my vm");

	WriteDWORD(0x0089C720, (DWORD) slf__destroy_debug_menu_entry__debug_menu_entry, "Hooking destroy_debug_menu_entry");
	WriteDWORD(0x0089C750, (DWORD) slf__debug_menu_entry__set_handler__str, "Hooking set_handler");

	//HookFunc(0x0054C89C, resource_pack_streamer_load_internal_hook, 0, "Hooking resource_pack_streamer::load_internal to inject interior loading");

	//HookFunc(0x005B87E0, os_developer_options, 1, "Hooking os_developer_options::get_flag");

	/*

	DWORD* windowHandler = 0x005AC48B;
	*windowHandler = WindowHandler;

	DWORD* otherHandler = 0x0076D6D1;
	*otherHandler = 0;

	*/
}

void dump_vtable(const char* name, DWORD* vtable) {
	printf("%s|%08X|%08X\n", name, vtable[0], vtable[1]);
}

void hook_slf_vtable(void* decons, void* action, DWORD* vtable) {
	vtable[0] = (DWORD)decons;
	vtable[1] = (DWORD)action;
}










    int __fastcall slf_deconstructor_abs_delay_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of abs_delay_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_abs_delay_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of abs_delay_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663170;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_acos_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of acos_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_acos_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of acos_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663F80;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_add_2d_debug_str_vector3d_vector3d_num_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of add_2d_debug_str_vector3d_vector3d_num_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_add_2d_debug_str_vector3d_vector3d_num_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of add_2d_debug_str_vector3d_vector3d_num_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663360;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_add_2d_debug_str_vector3d_vector3d_num_str_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of add_2d_debug_str_vector3d_vector3d_num_str_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

typedef struct vec3 {
        DWORD x, y, z;
} vec3;

#pragma pack(push, 1)
typedef struct add_2d_debug_str {
        vec3 unk[2];
        DWORD unk1;
        const char* name;
        DWORD unk2;
} add_2d_debug_str;
#pragma pack(pop)



int __fastcall slf_deconstructor_add_3d_debug_str_vector3d_vector3d_num_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of add_3d_debug_str_vector3d_vector3d_num_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_add_3d_debug_str_vector3d_vector3d_num_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of add_3d_debug_str_vector3d_vector3d_num_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663360;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_add_civilian_info_vector3d_num_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of add_civilian_info_vector3d_num_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_add_civilian_info_vector3d_num_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of add_civilian_info_vector3d_num_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00680FE0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_add_civilian_info_entity_entity_num_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of add_civilian_info_entity_entity_num_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_add_civilian_info_entity_entity_num_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of add_civilian_info_entity_entity_num_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006810F0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_add_debug_cyl_vector3d_vector3d_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of add_debug_cyl_vector3d_vector3d_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_add_debug_cyl_vector3d_vector3d_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of add_debug_cyl_vector3d_vector3d_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663390;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_add_debug_cyl_vector3d_vector3d_num_vector3d_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of add_debug_cyl_vector3d_vector3d_num_vector3d_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_add_debug_cyl_vector3d_vector3d_num_vector3d_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of add_debug_cyl_vector3d_vector3d_num_vector3d_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006633A0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_add_debug_line_vector3d_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of add_debug_line_vector3d_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_add_debug_line_vector3d_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of add_debug_line_vector3d_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663370;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_add_debug_line_vector3d_vector3d_vector3d_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of add_debug_line_vector3d_vector3d_vector3d_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_add_debug_line_vector3d_vector3d_vector3d_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of add_debug_line_vector3d_vector3d_vector3d_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663380;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_add_debug_sphere_vector3d_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of add_debug_sphere_vector3d_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_add_debug_sphere_vector3d_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of add_debug_sphere_vector3d_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00677930;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_add_debug_sphere_vector3d_num_vector3d_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of add_debug_sphere_vector3d_num_vector3d_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_add_debug_sphere_vector3d_num_vector3d_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of add_debug_sphere_vector3d_num_vector3d_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663360;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_add_glass_house_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of add_glass_house_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_add_glass_house_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of add_glass_house_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006798A0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_add_glass_house_str_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of add_glass_house_str_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_add_glass_house_str_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of add_glass_house_str_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00661FC0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_add_glass_house_str_num_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of add_glass_house_str_num_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_add_glass_house_str_num_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of add_glass_house_str_num_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662240;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_add_glass_house_str_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of add_glass_house_str_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_add_glass_house_str_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of add_glass_house_str_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662100;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_add_to_console_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of add_to_console_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_add_to_console_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of add_to_console_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_add_traffic_model_num_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of add_traffic_model_num_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_add_traffic_model_num_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of add_traffic_model_num_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00677770;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_allow_suspend_thread_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of allow_suspend_thread_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_allow_suspend_thread_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of allow_suspend_thread_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006627C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_angle_between_vector3d_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of angle_between_vector3d_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_angle_between_vector3d_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of angle_between_vector3d_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672070;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_apply_donut_damage_vector3d_num_num_num_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of apply_donut_damage_vector3d_num_num_num_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_apply_donut_damage_vector3d_num_num_num_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of apply_donut_damage_vector3d_num_num_num_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663460;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_apply_radius_damage_vector3d_num_num_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of apply_radius_damage_vector3d_num_num_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_apply_radius_damage_vector3d_num_num_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of apply_radius_damage_vector3d_num_num_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006633E0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_apply_radius_subdue_vector3d_num_num_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of apply_radius_subdue_vector3d_num_num_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_apply_radius_subdue_vector3d_num_num_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of apply_radius_subdue_vector3d_num_num_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006634E0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_assert_num_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of assert_num_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_assert_num_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of assert_num_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794D0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_attach_decal_str_vector3d_num_vector3d_entity(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of attach_decal_str_vector3d_num_vector3d_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_attach_decal_str_vector3d_num_vector3d_entity(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of attach_decal_str_vector3d_num_vector3d_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006643B0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_begin_screen_recording_str_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of begin_screen_recording_str_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_begin_screen_recording_str_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of begin_screen_recording_str_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067E740;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_blackscreen_off_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of blackscreen_off_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_blackscreen_off_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of blackscreen_off_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00673850;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_blackscreen_on_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of blackscreen_on_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_blackscreen_on_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of blackscreen_on_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00673800;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_bring_up_dialog_box_num_num_elip(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of bring_up_dialog_box_num_num_elip\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_bring_up_dialog_box_num_num_elip(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of bring_up_dialog_box_num_num_elip\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00673080;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_bring_up_dialog_box_debug_str_num_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of bring_up_dialog_box_debug_str_num_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_bring_up_dialog_box_debug_str_num_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of bring_up_dialog_box_debug_str_num_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00673490;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_bring_up_dialog_box_title_num_num_num_elip(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of bring_up_dialog_box_title_num_num_num_elip\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_bring_up_dialog_box_title_num_num_num_elip(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of bring_up_dialog_box_title_num_num_num_elip\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00673240;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_bring_up_medal_award_box_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of bring_up_medal_award_box_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_bring_up_medal_award_box_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of bring_up_medal_award_box_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006726E0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_bring_up_race_announcer(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of bring_up_race_announcer\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_bring_up_race_announcer(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of bring_up_race_announcer\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006726B0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_calc_launch_vector_vector3d_vector3d_num_entity(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of calc_launch_vector_vector3d_vector3d_num_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_calc_launch_vector_vector3d_vector3d_num_entity(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of calc_launch_vector_vector3d_vector3d_num_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006641D0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_can_load_pack_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of can_load_pack_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_can_load_pack_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of can_load_pack_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00680C60;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_chase_cam(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of chase_cam\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_chase_cam(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of chase_cam\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067BBD0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_clear_all_grenades(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of clear_all_grenades\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_clear_all_grenades(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of clear_all_grenades\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006640D0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_clear_civilians_within_radius_vector3d_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of clear_civilians_within_radius_vector3d_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_clear_civilians_within_radius_vector3d_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of clear_civilians_within_radius_vector3d_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00677930;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_clear_controls(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of clear_controls\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_clear_controls(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of clear_controls\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00673B70;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_clear_debug_all(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of clear_debug_all\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_clear_debug_all(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of clear_debug_all\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794B0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_clear_debug_cyls(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of clear_debug_cyls\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_clear_debug_cyls(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of clear_debug_cyls\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794B0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_clear_debug_lines(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of clear_debug_lines\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_clear_debug_lines(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of clear_debug_lines\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794B0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_clear_debug_spheres(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of clear_debug_spheres\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_clear_debug_spheres(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of clear_debug_spheres\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794B0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_clear_screen(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of clear_screen\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_clear_screen(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of clear_screen\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006640C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_clear_traffic_within_radius_vector3d_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of clear_traffic_within_radius_vector3d_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_clear_traffic_within_radius_vector3d_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of clear_traffic_within_radius_vector3d_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006778F0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_col_check_vector3d_vector3d_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of col_check_vector3d_vector3d_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_col_check_vector3d_vector3d_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of col_check_vector3d_vector3d_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663770;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_console_exec_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of console_exec_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_console_exec_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of console_exec_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_copy_vector3d_list_vector3d_list_vector3d_list(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of copy_vector3d_list_vector3d_list_vector3d_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_copy_vector3d_list_vector3d_list_vector3d_list(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of copy_vector3d_list_vector3d_list_vector3d_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00686E00;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_cos_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of cos_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_cos_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of cos_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663F20;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_beam(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_beam\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_beam(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_beam\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067AD20;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_credits(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_credits\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_credits(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_credits\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672630;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_cut_scene_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_cut_scene_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_cut_scene_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_cut_scene_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00670AF0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_debug_menu_entry_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_debug_menu_entry_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_debug_menu_entry_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_debug_menu_entry_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067C1E0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_debug_menu_entry_str_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_debug_menu_entry_str_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_debug_menu_entry_str_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_debug_menu_entry_str_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00678210;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_decal_str_vector3d_num_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_decal_str_vector3d_num_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_decal_str_vector3d_num_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_decal_str_vector3d_num_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00664340;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_entity_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_entity_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}



int __fastcall slf_deconstructor_create_entity_str_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_entity_str_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_entity_str_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_entity_str_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067BD40;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_entity_in_hero_region_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_entity_in_hero_region_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_entity_in_hero_region_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_entity_in_hero_region_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067BEC0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_entity_list(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_entity_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_entity_list(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_entity_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006860D0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_entity_tracker_entity(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_entity_tracker_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_entity_tracker_entity(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_entity_tracker_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00677650;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_item_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_item_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_item_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_item_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067E190;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_line_info_vector3d_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_line_info_vector3d_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_line_info_vector3d_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_line_info_vector3d_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067E440;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_lofi_stereo_sound_inst_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_lofi_stereo_sound_inst_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_lofi_stereo_sound_inst_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_lofi_stereo_sound_inst_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067EBF0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_num_list(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_num_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_num_list(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_num_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00685FD0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_pfx_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_pfx_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_pfx_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_pfx_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00678E20;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_pfx_str_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_pfx_str_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_pfx_str_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_pfx_str_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00678F30;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_polytube(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_polytube\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_polytube(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_polytube\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00680510;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_polytube_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_polytube_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_polytube_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_polytube_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006805E0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_progression_menu_entry_str_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_progression_menu_entry_str_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_progression_menu_entry_str_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_progression_menu_entry_str_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00678210;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_sound_inst(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_sound_inst\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_sound_inst(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_sound_inst\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067E840;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_sound_inst_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_sound_inst_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_sound_inst_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_sound_inst_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067E920;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_stompable_music_sound_inst_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_stompable_music_sound_inst_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_stompable_music_sound_inst_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_stompable_music_sound_inst_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067EA10;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_str_list(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_str_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_str_list(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_str_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00686180;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_taunt_entry_entity_str_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_taunt_entry_entity_str_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_taunt_entry_entity_str_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_taunt_entry_entity_str_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00677B80;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_taunt_exchange_entity_entity_num_num_num_num_elip(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_taunt_exchange_entity_entity_num_num_num_num_elip\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_taunt_exchange_entity_entity_num_num_num_num_elip(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_taunt_exchange_entity_entity_num_num_num_num_elip\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00686330;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_taunt_exchange_list(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_taunt_exchange_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_taunt_exchange_list(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_taunt_exchange_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00686260;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_threat_assessment_meter(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_threat_assessment_meter\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_threat_assessment_meter(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_threat_assessment_meter\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00678030;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_time_limited_entity_str_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_time_limited_entity_str_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_time_limited_entity_str_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_time_limited_entity_str_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00668C60;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_trigger_entity_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_trigger_entity_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_trigger_entity_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_trigger_entity_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067FC50;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_trigger_str_vector3d_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_trigger_str_vector3d_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_trigger_str_vector3d_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_trigger_str_vector3d_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067FB60;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_trigger_vector3d_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_trigger_vector3d_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_trigger_vector3d_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_trigger_vector3d_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067FA80;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_unstompable_script_cutscene_sound_inst_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_unstompable_script_cutscene_sound_inst_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_unstompable_script_cutscene_sound_inst_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_unstompable_script_cutscene_sound_inst_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067EB00;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_create_vector3d_list(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of create_vector3d_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_create_vector3d_list(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of create_vector3d_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00685F20;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_cross_vector3d_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of cross_vector3d_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_cross_vector3d_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of cross_vector3d_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00671F70;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_debug_breakpoint(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of debug_breakpoint\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_debug_breakpoint(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of debug_breakpoint\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794B0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_debug_print_num_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of debug_print_num_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_debug_print_num_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of debug_print_num_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794D0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_debug_print_num_vector3d_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of debug_print_num_vector3d_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_debug_print_num_vector3d_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of debug_print_num_vector3d_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00661F50;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_debug_print_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of debug_print_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_debug_print_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of debug_print_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_debug_print_set_background_color_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of debug_print_set_background_color_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_debug_print_set_background_color_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of debug_print_set_background_color_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00661F60;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_delay_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of delay_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_delay_num(void* _this, void* edx, void* arg, void* arg1)
{

        // printf("Calling action of delay_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663120;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_destroy_credits(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of destroy_credits\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_destroy_credits(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of destroy_credits\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672650;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_destroy_debug_menu_entry_debug_menu_entry(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of destroy_debug_menu_entry_debug_menu_entry\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_destroy_debug_menu_entry_debug_menu_entry(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of destroy_debug_menu_entry_debug_menu_entry\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_destroy_entity_entity(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of destroy_entity_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_destroy_entity_entity(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of destroy_entity_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067C010;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_destroy_entity_list_entity_list(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of destroy_entity_list_entity_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_destroy_entity_list_entity_list(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of destroy_entity_list_entity_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00686F10;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_destroy_entity_tracker_entity_tracker(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of destroy_entity_tracker_entity_tracker\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_destroy_entity_tracker_entity_tracker(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of destroy_entity_tracker_entity_tracker\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00677720;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_destroy_line_info_line_info(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of destroy_line_info_line_info\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_destroy_line_info_line_info(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of destroy_line_info_line_info\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006705C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_destroy_num_list_num_list(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of destroy_num_list_num_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_destroy_num_list_num_list(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of destroy_num_list_num_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00686080;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_destroy_pfx_pfx(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of destroy_pfx_pfx\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_destroy_pfx_pfx(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of destroy_pfx_pfx\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00687870;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_destroy_str_list_str_list(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of destroy_str_list_str_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_destroy_str_list_str_list(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of destroy_str_list_str_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006878B0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_destroy_taunt_entry_taunt_entry(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of destroy_taunt_entry_taunt_entry\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_destroy_taunt_entry_taunt_entry(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of destroy_taunt_entry_taunt_entry\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00677C70;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_destroy_taunt_exchange_taunt_exchange(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of destroy_taunt_exchange_taunt_exchange\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_destroy_taunt_exchange_taunt_exchange(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of destroy_taunt_exchange_taunt_exchange\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00686B90;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_destroy_taunt_exchange_list_taunt_exchange_list(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of destroy_taunt_exchange_list_taunt_exchange_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_destroy_taunt_exchange_list_taunt_exchange_list(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of destroy_taunt_exchange_list_taunt_exchange_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00686B00;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_destroy_threat_assessment_meter_tam(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of destroy_threat_assessment_meter_tam\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_destroy_threat_assessment_meter_tam(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of destroy_threat_assessment_meter_tam\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00678060;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_destroy_trigger_trigger(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of destroy_trigger_trigger\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_destroy_trigger_trigger(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of destroy_trigger_trigger\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067FD20;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_destroy_vector3d_list_vector3d_list(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of destroy_vector3d_list_vector3d_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_destroy_vector3d_list_vector3d_list(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of destroy_vector3d_list_vector3d_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00686E30;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_dilated_delay_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of dilated_delay_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_dilated_delay_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of dilated_delay_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006631D0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_disable_marky_cam_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of disable_marky_cam_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_disable_marky_cam_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of disable_marky_cam_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00679A00;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_disable_nearby_occlusion_only_obb_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of disable_nearby_occlusion_only_obb_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_disable_nearby_occlusion_only_obb_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of disable_nearby_occlusion_only_obb_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662AA0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_disable_player_shadows(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of disable_player_shadows\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_disable_player_shadows(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of disable_player_shadows\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662B10;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_disable_subtitles(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of disable_subtitles\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_disable_subtitles(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of disable_subtitles\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006640B0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_disable_vibrator(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of disable_vibrator\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_disable_vibrator(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of disable_vibrator\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067A600;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_disable_zoom_map_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of disable_zoom_map_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_disable_zoom_map_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of disable_zoom_map_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672BB0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_distance3d_vector3d_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of distance3d_vector3d_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_distance3d_vector3d_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of distance3d_vector3d_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672010;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_distance_chase_widget_set_pos_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of distance_chase_widget_set_pos_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_distance_chase_widget_set_pos_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of distance_chase_widget_set_pos_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006729F0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_distance_chase_widget_turn_off(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of distance_chase_widget_turn_off\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_distance_chase_widget_turn_off(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of distance_chase_widget_turn_off\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006729D0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_distance_chase_widget_turn_on_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of distance_chase_widget_turn_on_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_distance_chase_widget_turn_on_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of distance_chase_widget_turn_on_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672980;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_distance_race_widget_set_boss_pos_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of distance_race_widget_set_boss_pos_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_distance_race_widget_set_boss_pos_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of distance_race_widget_set_boss_pos_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672A90;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_distance_race_widget_set_hero_pos_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of distance_race_widget_set_hero_pos_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_distance_race_widget_set_hero_pos_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of distance_race_widget_set_hero_pos_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672A60;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_distance_race_widget_set_types_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of distance_race_widget_set_types_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_distance_race_widget_set_types_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of distance_race_widget_set_types_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672AC0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_distance_race_widget_turn_off(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of distance_race_widget_turn_off\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_distance_race_widget_turn_off(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of distance_race_widget_turn_off\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672A40;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_distance_race_widget_turn_on(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of distance_race_widget_turn_on\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_distance_race_widget_turn_on(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of distance_race_widget_turn_on\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672A20;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_district_id_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of district_id_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_district_id_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of district_id_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006769B0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_district_name_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of district_name_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_district_name_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of district_name_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00680F70;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_dot_vector3d_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of dot_vector3d_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_dot_vector3d_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of dot_vector3d_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00671F30;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_dump_searchable_region_list_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of dump_searchable_region_list_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_dump_searchable_region_list_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of dump_searchable_region_list_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_enable_ai_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of enable_ai_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_enable_ai_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of enable_ai_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662F90;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_enable_civilians_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of enable_civilians_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_enable_civilians_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of enable_civilians_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00677940;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_enable_controls_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of enable_controls_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_enable_controls_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of enable_controls_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00673AF0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_enable_entity_fading_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of enable_entity_fading_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_enable_entity_fading_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of enable_entity_fading_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00669AC0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_enable_interface_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of enable_interface_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_enable_interface_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of enable_interface_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662F40;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_enable_marky_cam_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of enable_marky_cam_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_enable_marky_cam_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of enable_marky_cam_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006799B0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_enable_mini_map_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of enable_mini_map_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_enable_mini_map_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of enable_mini_map_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672B60;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_enable_nearby_occlusion_only_obb_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of enable_nearby_occlusion_only_obb_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_enable_nearby_occlusion_only_obb_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of enable_nearby_occlusion_only_obb_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662A70;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_enable_obb_vector3d_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of enable_obb_vector3d_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_enable_obb_vector3d_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of enable_obb_vector3d_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662690;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_enable_pause_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of enable_pause_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_enable_pause_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of enable_pause_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662E50;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_enable_physics_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of enable_physics_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_enable_physics_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of enable_physics_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663020;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_enable_player_shadows(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of enable_player_shadows\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_enable_player_shadows(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of enable_player_shadows\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662B20;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_enable_pois_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of enable_pois_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_enable_pois_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of enable_pois_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_enable_quad_path_connector_district_num_district_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of enable_quad_path_connector_district_num_district_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_enable_quad_path_connector_district_num_district_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of enable_quad_path_connector_district_num_district_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662510;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_enable_subtitles(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of enable_subtitles\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_enable_subtitles(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of enable_subtitles\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006640A0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_enable_tokens_of_type_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of enable_tokens_of_type_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_enable_tokens_of_type_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of enable_tokens_of_type_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0066F420;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_enable_traffic_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of enable_traffic_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_enable_traffic_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of enable_traffic_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006779C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_enable_user_camera_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of enable_user_camera_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_enable_user_camera_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of enable_user_camera_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662A10;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_enable_vibrator(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of enable_vibrator\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_enable_vibrator(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of enable_vibrator\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067A620;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_end_current_patrol(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of end_current_patrol\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_end_current_patrol(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of end_current_patrol\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00676EC0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_end_cut_scenes(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of end_cut_scenes\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_end_cut_scenes(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of end_cut_scenes\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00670C80;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_end_screen_recording(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of end_screen_recording\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_end_screen_recording(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of end_screen_recording\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067E7E0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_entity_col_check_entity_entity(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of entity_col_check_entity_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_entity_col_check_entity_entity(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of entity_col_check_entity_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006639D0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_entity_exists_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of entity_exists_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}



int __fastcall slf_deconstructor_entity_get_entity_tracker_entity(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of entity_get_entity_tracker_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_entity_get_entity_tracker_entity(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of entity_get_entity_tracker_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006697F0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_entity_has_entity_tracker_entity(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of entity_has_entity_tracker_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_entity_has_entity_tracker_entity(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of entity_has_entity_tracker_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00669790;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_exit_water_entity(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of exit_water_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_exit_water_entity(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of exit_water_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00682380;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_find_closest_point_on_a_path_to_point_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of find_closest_point_on_a_path_to_point_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_find_closest_point_on_a_path_to_point_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of find_closest_point_on_a_path_to_point_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006624B0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_find_district_for_point_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of find_district_for_point_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_find_district_for_point_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of find_district_for_point_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663560;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_find_entities_in_radius_entity_list_vector3d_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of find_entities_in_radius_entity_list_vector3d_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_find_entities_in_radius_entity_list_vector3d_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of find_entities_in_radius_entity_list_vector3d_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00686C60;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_find_entity_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of find_entity_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_find_entity_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of find_entity_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00668B90;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_find_innermost_district_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of find_innermost_district_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_find_innermost_district_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of find_innermost_district_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006635A0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_find_outermost_district_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of find_outermost_district_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_find_outermost_district_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of find_outermost_district_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006635E0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_find_trigger_entity(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of find_trigger_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_find_trigger_entity(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of find_trigger_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067F9B0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_find_trigger_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of find_trigger_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_find_trigger_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of find_trigger_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067F900;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_find_trigger_in_district_district_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of find_trigger_in_district_district_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_find_trigger_in_district_district_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of find_trigger_in_district_district_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067FA00;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_float_random_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of float_random_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_float_random_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of float_random_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663300;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_force_mission_district_str_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of force_mission_district_str_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_force_mission_district_str_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of force_mission_district_str_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006765D0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_force_streamer_refresh(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of force_streamer_refresh\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_force_streamer_refresh(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of force_streamer_refresh\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00676B70;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_format_time_string_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of format_time_string_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_format_time_string_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of format_time_string_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006736A0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_freeze_hero_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of freeze_hero_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_freeze_hero_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of freeze_hero_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00679A50;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_game_ini_get_flag_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of game_ini_get_flag_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_game_ini_get_flag_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of game_ini_get_flag_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067AC20;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_game_time_advance_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of game_time_advance_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_game_time_advance_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of game_time_advance_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00676D30;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_all_execs_thread_count_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_all_execs_thread_count_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_all_execs_thread_count_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_all_execs_thread_count_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006824D0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_all_instances_thread_count_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_all_instances_thread_count_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_all_instances_thread_count_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_all_instances_thread_count_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00681500;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_attacker_entity(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_attacker_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_attacker_entity(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_attacker_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006644F0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_attacker_member(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_attacker_member\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_attacker_member(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_attacker_member\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006644F0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_available_stack_size(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_available_stack_size\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_available_stack_size(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_available_stack_size\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00676E30;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_character_packname_list(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_character_packname_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_character_packname_list(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_character_packname_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00676270;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_closest_point_on_lane_with_facing_num_vector3d_vector3d_list(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_closest_point_on_lane_with_facing_num_vector3d_vector3d_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_closest_point_on_lane_with_facing_num_vector3d_vector3d_list(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_closest_point_on_lane_with_facing_num_vector3d_vector3d_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00687180;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_col_hit_ent(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_col_hit_ent\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_col_hit_ent(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_col_hit_ent\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067A6E0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_col_hit_norm(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_col_hit_norm\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_col_hit_norm(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_col_hit_norm\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663990;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_col_hit_pos(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_col_hit_pos\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_col_hit_pos(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_col_hit_pos\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663950;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_control_state_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_control_state_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_control_state_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_control_state_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067A690;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_control_trigger_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_control_trigger_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_control_trigger_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_control_trigger_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067A640;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_current_instance_thread_count_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_current_instance_thread_count_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_current_instance_thread_count_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_current_instance_thread_count_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00681490;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_current_view_cam_pos(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_current_view_cam_pos\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_current_view_cam_pos(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_current_view_cam_pos\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662930;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_current_view_cam_x_facing(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_current_view_cam_x_facing\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_current_view_cam_x_facing(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_current_view_cam_x_facing\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662810;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_current_view_cam_y_facing(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_current_view_cam_y_facing\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_current_view_cam_y_facing(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_current_view_cam_y_facing\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662870;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_current_view_cam_z_facing(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_current_view_cam_z_facing\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_current_view_cam_z_facing(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_current_view_cam_z_facing\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006628D0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_fog_color(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_fog_color\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_fog_color(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_fog_color\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663E60;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_fog_distance(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_fog_distance\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_fog_distance(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_fog_distance\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663EC0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_game_info_num_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_game_info_num_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_game_info_num_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_game_info_num_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663C00;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_game_info_str_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_game_info_str_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_game_info_str_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_game_info_str_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067A7F0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_glam_cam_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_glam_cam_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_glam_cam_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_glam_cam_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00670930;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_global_time_dilation(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_global_time_dilation\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_global_time_dilation(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_global_time_dilation\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663A60;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_ini_flag_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_ini_flag_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_ini_flag_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_ini_flag_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067AC20;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_ini_num_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_ini_num_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_ini_num_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_ini_num_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067AB70;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_int_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_int_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_int_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_int_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00664060;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_mission_camera_marker_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_mission_camera_marker_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_mission_camera_marker_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_mission_camera_marker_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00682F80;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_mission_camera_transform_marker_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_mission_camera_transform_marker_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_mission_camera_transform_marker_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_mission_camera_transform_marker_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00683040;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_mission_entity(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_mission_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_mission_entity(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_mission_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006764E0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_mission_key_posfacing3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_mission_key_posfacing3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_mission_key_posfacing3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_mission_key_posfacing3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00676330;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_mission_key_position(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_mission_key_position\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_mission_key_position(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_mission_key_position\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006762E0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_mission_marker_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_mission_marker_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_mission_marker_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_mission_marker_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00682F10;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_mission_nums(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_mission_nums\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_mission_nums(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_mission_nums\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006765A0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_mission_patrol_waypoint(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_mission_patrol_waypoint\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_mission_patrol_waypoint(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_mission_patrol_waypoint\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006764A0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_mission_positions(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_mission_positions\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_mission_positions(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_mission_positions\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00676540;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_mission_strings(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_mission_strings\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_mission_strings(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_mission_strings\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00676570;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_mission_transform_marker_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_mission_transform_marker_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_mission_transform_marker_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_mission_transform_marker_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00682FE0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_mission_trigger(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_mission_trigger\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_mission_trigger(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_mission_trigger\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00676510;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_missions_key_position_by_index_district_str_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_missions_key_position_by_index_district_str_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_missions_key_position_by_index_district_str_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_missions_key_position_by_index_district_str_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00676620;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_missions_nums_by_index_district_str_num_num_list(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_missions_nums_by_index_district_str_num_num_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_missions_nums_by_index_district_str_num_num_list(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_missions_nums_by_index_district_str_num_num_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00682EB0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_missions_patrol_waypoint_by_index_district_str_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_missions_patrol_waypoint_by_index_district_str_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_missions_patrol_waypoint_by_index_district_str_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_missions_patrol_waypoint_by_index_district_str_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006766A0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_neighborhood_name_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_neighborhood_name_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_neighborhood_name_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_neighborhood_name_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00677220;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_num_free_slots_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_num_free_slots_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_num_free_slots_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_num_free_slots_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00676720;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_num_mission_transform_marker(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_num_mission_transform_marker\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_num_mission_transform_marker(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_num_mission_transform_marker\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00680F10;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_pack_group_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_pack_group_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_pack_group_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_pack_group_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00680B80;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_pack_size_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_pack_size_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_pack_size_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_pack_size_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00676D70;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_patrol_difficulty_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_patrol_difficulty_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_patrol_difficulty_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_patrol_difficulty_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00677100;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_patrol_node_position_by_index_str_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_patrol_node_position_by_index_str_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_patrol_node_position_by_index_str_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_patrol_node_position_by_index_str_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00677080;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_patrol_start_position_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_patrol_start_position_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_patrol_start_position_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_patrol_start_position_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00677010;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_patrol_unlock_threshold_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_patrol_unlock_threshold_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_patrol_unlock_threshold_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_patrol_unlock_threshold_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00677160;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_platform(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_platform\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_platform(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_platform\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00661F20;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_render_opt_num_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_render_opt_num_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_render_opt_num_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_render_opt_num_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663DD0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_spider_reflexes_spiderman_time_dilation(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_spider_reflexes_spiderman_time_dilation\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_spider_reflexes_spiderman_time_dilation(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_spider_reflexes_spiderman_time_dilation\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00679390;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_spider_reflexes_world_time_dilation(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_spider_reflexes_world_time_dilation\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_spider_reflexes_world_time_dilation(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_spider_reflexes_world_time_dilation\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00679390;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_time_inc(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_time_inc\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_time_inc(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_time_inc\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006633B0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_time_of_day(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_time_of_day\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_time_of_day(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_time_of_day\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00664150;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_time_of_day_rate(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_time_of_day_rate\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_time_of_day_rate(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_time_of_day_rate\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00664110;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_token_index_from_id_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_token_index_from_id_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_token_index_from_id_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_token_index_from_id_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0066F470;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_get_traffic_spawn_point_near_camera_vector3d_list(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of get_traffic_spawn_point_near_camera_vector3d_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_get_traffic_spawn_point_near_camera_vector3d_list(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of get_traffic_spawn_point_near_camera_vector3d_list\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00686BE0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_greater_than_or_equal_rounded_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of greater_than_or_equal_rounded_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_greater_than_or_equal_rounded_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of greater_than_or_equal_rounded_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00673750;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_hard_break(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of hard_break\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_hard_break(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of hard_break\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794B0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_has_substring_str_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of has_substring_str_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_has_substring_str_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of has_substring_str_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006625D0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_hero(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of hero\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_hero(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of hero\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067BBA0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_hero_exists(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of hero_exists\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_hero_exists(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of hero_exists\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00668A10;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_hero_type(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of hero_type\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_hero_type(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of hero_type\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00668A50;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_hide_controller_gauge(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of hide_controller_gauge\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_hide_controller_gauge(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of hide_controller_gauge\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672750;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_initialize_encounter_object(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of initialize_encounter_object\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_initialize_encounter_object(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of initialize_encounter_object\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00664490;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_initialize_encounter_objects(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of initialize_encounter_objects\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_initialize_encounter_objects(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of initialize_encounter_objects\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00664430;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_insert_pack_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of insert_pack_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_insert_pack_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of insert_pack_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006809A0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_invoke_pause_menu_unlockables(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of invoke_pause_menu_unlockables\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_invoke_pause_menu_unlockables(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of invoke_pause_menu_unlockables\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006737C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_is_ai_enabled(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of is_ai_enabled\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_is_ai_enabled(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of is_ai_enabled\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662FE0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_is_cut_scene_playing(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of is_cut_scene_playing\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_is_cut_scene_playing(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of is_cut_scene_playing\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00670CA0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_is_district_loaded_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of is_district_loaded_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_is_district_loaded_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of is_district_loaded_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00676AB0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_is_hero_frozen(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of is_hero_frozen\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_is_hero_frozen(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of is_hero_frozen\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00679AA0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_is_hero_peter_parker(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of is_hero_peter_parker\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_is_hero_peter_parker(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of is_hero_peter_parker\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00668B40;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_is_hero_spidey(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of is_hero_spidey\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_is_hero_spidey(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of is_hero_spidey\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00668AA0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_is_hero_venom(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of is_hero_venom\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_is_hero_venom(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of is_hero_venom\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00668AF0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_is_marky_cam_enabled(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of is_marky_cam_enabled\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_is_marky_cam_enabled(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of is_marky_cam_enabled\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662990;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_is_mission_active(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of is_mission_active\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_is_mission_active(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of is_mission_active\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00676F20;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_is_mission_loading(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of is_mission_loading\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_is_mission_loading(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of is_mission_loading\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00676F60;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_is_pack_available_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of is_pack_available_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_is_pack_available_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of is_pack_available_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00680E40;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_is_pack_loaded_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of is_pack_loaded_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_is_pack_loaded_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of is_pack_loaded_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00680D50;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_is_pack_pushed_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of is_pack_pushed_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_is_pack_pushed_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of is_pack_pushed_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00676190;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_is_path_graph_inside_glass_house_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of is_path_graph_inside_glass_house_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_is_path_graph_inside_glass_house_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of is_path_graph_inside_glass_house_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00664590;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_is_patrol_active(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of is_patrol_active\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_is_patrol_active(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of is_patrol_active\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00676EE0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_is_patrol_node_empty_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of is_patrol_node_empty_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_is_patrol_node_empty_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of is_patrol_node_empty_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006771C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_is_paused(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of is_paused\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_is_paused(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of is_paused\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662EA0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_is_physics_enabled(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of is_physics_enabled\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_is_physics_enabled(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of is_physics_enabled\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663070;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_is_point_inside_glass_house_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of is_point_inside_glass_house_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_is_point_inside_glass_house_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of is_point_inside_glass_house_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00661F70;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_is_point_under_water_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of is_point_under_water_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_is_point_under_water_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of is_point_under_water_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00664540;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_is_user_camera_enabled(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of is_user_camera_enabled\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_is_user_camera_enabled(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of is_user_camera_enabled\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006629D0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_load_anim_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of load_anim_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_load_anim_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of load_anim_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_load_level_str_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of load_level_str_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_load_level_str_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of load_level_str_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663AD0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_lock_all_districts(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of lock_all_districts\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_lock_all_districts(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of lock_all_districts\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00676C10;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_lock_district_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of lock_district_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_lock_district_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of lock_district_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00676A70;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_lock_mission_manager_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of lock_mission_manager_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_lock_mission_manager_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of lock_mission_manager_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00676FA0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_los_check_vector3d_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of los_check_vector3d_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_los_check_vector3d_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of los_check_vector3d_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006813E0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_lower_hotpursuit_indicator_level(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of lower_hotpursuit_indicator_level\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_lower_hotpursuit_indicator_level(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of lower_hotpursuit_indicator_level\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672610;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_malor_vector3d_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of malor_vector3d_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_malor_vector3d_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of malor_vector3d_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00664180;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_normal_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of normal_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_normal_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of normal_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00671FD0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_pause_game_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of pause_game_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_pause_game_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of pause_game_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662EE0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_play_credits(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of play_credits\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_play_credits(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of play_credits\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672670;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_play_prerender_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of play_prerender_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_play_prerender_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of play_prerender_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663B60;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_pop_pack_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of pop_pack_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_pop_pack_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of pop_pack_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00680870;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_post_message_str_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of post_message_str_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_post_message_str_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of post_message_str_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067A460;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_pre_roll_all_pfx_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of pre_roll_all_pfx_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_pre_roll_all_pfx_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of pre_roll_all_pfx_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00664510;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_press_controller_gauge_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of press_controller_gauge_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_press_controller_gauge_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of press_controller_gauge_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672770;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_press_controller_gauge_num_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of press_controller_gauge_num_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_press_controller_gauge_num_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of press_controller_gauge_num_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006727A0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_purge_district_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of purge_district_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_purge_district_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of purge_district_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00676BD0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_push_pack_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of push_pack_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_push_pack_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of push_pack_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00680730;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_push_pack_into_district_slot_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of push_pack_into_district_slot_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_push_pack_into_district_slot_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of push_pack_into_district_slot_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00682B80;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_raise_hotpursuit_indicator_level(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of raise_hotpursuit_indicator_level\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_raise_hotpursuit_indicator_level(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of raise_hotpursuit_indicator_level\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006725F0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_random_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of random_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_random_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of random_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663280;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_remove_civilian_info_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of remove_civilian_info_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_remove_civilian_info_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of remove_civilian_info_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00677870;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_remove_civilian_info_entity_entity_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of remove_civilian_info_entity_entity_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_remove_civilian_info_entity_entity_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of remove_civilian_info_entity_entity_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00681240;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_remove_glass_house_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of remove_glass_house_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_remove_glass_house_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of remove_glass_house_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006623A0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_remove_item_entity_from_world_entity(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of remove_item_entity_from_world_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_remove_item_entity_from_world_entity(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of remove_item_entity_from_world_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00669500;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_remove_pack_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of remove_pack_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_remove_pack_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of remove_pack_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00680A90;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_remove_traffic_model_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of remove_traffic_model_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_remove_traffic_model_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of remove_traffic_model_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00677810;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_reset_externed_alses(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of reset_externed_alses\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_reset_externed_alses(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of reset_externed_alses\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00669B00;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_all_anchors_activated_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_all_anchors_activated_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_all_anchors_activated_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_all_anchors_activated_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006830A0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_blur_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_blur_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_blur_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_blur_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662B90;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_blur_blend_mode_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_blur_blend_mode_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_blur_blend_mode_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_blur_blend_mode_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662CC0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_blur_color_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_blur_color_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_blur_color_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_blur_color_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662BC0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_blur_offset_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_blur_offset_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_blur_offset_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_blur_offset_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662C50;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_blur_rot_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_blur_rot_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_blur_rot_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_blur_rot_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662C90;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_blur_scale_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_blur_scale_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_blur_scale_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_blur_scale_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662C10;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_clear_color_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_clear_color_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_clear_color_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_clear_color_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006630B0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_current_mission_objective_caption_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_current_mission_objective_caption_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_current_mission_objective_caption_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_current_mission_objective_caption_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00679570;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_default_traffic_hitpoints_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_default_traffic_hitpoints_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_default_traffic_hitpoints_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_default_traffic_hitpoints_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00677A70;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_dialog_box_flavor_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_dialog_box_flavor_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_dialog_box_flavor_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_dialog_box_flavor_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00673600;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_dialog_box_lockout_time_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_dialog_box_lockout_time_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_dialog_box_lockout_time_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_dialog_box_lockout_time_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00673670;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_engine_property_str_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_engine_property_str_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_engine_property_str_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_engine_property_str_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006642A0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_fov_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_fov_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_fov_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_fov_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662B60;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_game_info_num_str_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_game_info_num_str_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_game_info_num_str_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_game_info_num_str_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663B90;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_game_info_str_str_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_game_info_str_str_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_game_info_str_str_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_game_info_str_str_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663C90;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_global_time_dilation_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_global_time_dilation_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_global_time_dilation_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_global_time_dilation_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663A90;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_marky_cam_lookat_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_marky_cam_lookat_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_marky_cam_lookat_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_marky_cam_lookat_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662AD0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_max_streaming_distance_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_max_streaming_distance_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_max_streaming_distance_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_max_streaming_distance_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00676B90;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_mission_key_pos_facing_vector3d_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_mission_key_pos_facing_vector3d_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_mission_key_pos_facing_vector3d_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_mission_key_pos_facing_vector3d_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00676430;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_mission_key_position_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_mission_key_position_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_mission_key_position_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_mission_key_position_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00676390;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_mission_text_num_elip(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_mission_text_num_elip\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_mission_text_num_elip(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_mission_text_num_elip\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672C00;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_mission_text_box_flavor_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_mission_text_box_flavor_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_mission_text_box_flavor_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_mission_text_box_flavor_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00673640;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_mission_text_debug_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_mission_text_debug_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_mission_text_debug_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_mission_text_debug_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672FA0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_parking_density_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_parking_density_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_parking_density_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_parking_density_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00677A40;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_pedestrian_density_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_pedestrian_density_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_pedestrian_density_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_pedestrian_density_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00677990;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_render_opt_num_str_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_render_opt_num_str_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_render_opt_num_str_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_render_opt_num_str_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663D60;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_score_widget_score_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_score_widget_score_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_score_widget_score_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_score_widget_score_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672570;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_sound_category_volume_num_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_sound_category_volume_num_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_sound_category_volume_num_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_sound_category_volume_num_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00664300;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_spider_reflexes_blur_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_spider_reflexes_blur_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_spider_reflexes_blur_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_spider_reflexes_blur_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662CF0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_spider_reflexes_blur_blend_mode_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_spider_reflexes_blur_blend_mode_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_spider_reflexes_blur_blend_mode_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_spider_reflexes_blur_blend_mode_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662E20;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_spider_reflexes_blur_color_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_spider_reflexes_blur_color_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_spider_reflexes_blur_color_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_spider_reflexes_blur_color_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662D20;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_spider_reflexes_blur_offset_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_spider_reflexes_blur_offset_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_spider_reflexes_blur_offset_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_spider_reflexes_blur_offset_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662DB0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_spider_reflexes_blur_rot_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_spider_reflexes_blur_rot_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_spider_reflexes_blur_rot_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_spider_reflexes_blur_rot_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662DF0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_spider_reflexes_blur_scale_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_spider_reflexes_blur_scale_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_spider_reflexes_blur_scale_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_spider_reflexes_blur_scale_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662D70;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_spider_reflexes_hero_meter_depletion_rate_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_spider_reflexes_hero_meter_depletion_rate_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_spider_reflexes_hero_meter_depletion_rate_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_spider_reflexes_hero_meter_depletion_rate_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_spider_reflexes_spiderman_time_dilation_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_spider_reflexes_spiderman_time_dilation_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_spider_reflexes_spiderman_time_dilation_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_spider_reflexes_spiderman_time_dilation_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_spider_reflexes_world_time_dilation_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_spider_reflexes_world_time_dilation_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_spider_reflexes_world_time_dilation_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_spider_reflexes_world_time_dilation_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_state_of_the_story_caption_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_state_of_the_story_caption_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_state_of_the_story_caption_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_state_of_the_story_caption_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00679570;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_target_info_entity_vector3d_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_target_info_entity_vector3d_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_target_info_entity_vector3d_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_target_info_entity_vector3d_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067E3C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_time_of_day_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_time_of_day_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_time_of_day_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_time_of_day_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006640E0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_traffic_density_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_traffic_density_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_traffic_density_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_traffic_density_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00677A10;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_traffic_model_usage_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_traffic_model_usage_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_traffic_model_usage_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_traffic_model_usage_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00677840;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_vibration_resume_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_vibration_resume_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_vibration_resume_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_vibration_resume_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067A5B0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_whoosh_interp_rate_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_whoosh_interp_rate_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_whoosh_interp_rate_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_whoosh_interp_rate_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0066EBE0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_whoosh_pitch_range_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_whoosh_pitch_range_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_whoosh_pitch_range_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_whoosh_pitch_range_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0066EBB0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_whoosh_speed_range_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_whoosh_speed_range_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_whoosh_speed_range_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_whoosh_speed_range_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0066EB50;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_whoosh_volume_range_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_whoosh_volume_range_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_whoosh_volume_range_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_whoosh_volume_range_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0066EB80;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_set_zoom_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of set_zoom_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_set_zoom_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of set_zoom_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00662B30;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_show_controller_gauge(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of show_controller_gauge\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_show_controller_gauge(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of show_controller_gauge\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672730;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_show_hotpursuit_indicator_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of show_hotpursuit_indicator_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_show_hotpursuit_indicator_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of show_hotpursuit_indicator_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006725A0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_show_score_widget_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of show_score_widget_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_show_score_widget_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of show_score_widget_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672520;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_shut_up_all_ai_voice_boxes(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of shut_up_all_ai_voice_boxes\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_shut_up_all_ai_voice_boxes(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of shut_up_all_ai_voice_boxes\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0066EC10;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_sin_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of sin_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_sin_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of sin_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663EF0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_sin_cos_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of sin_cos_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_sin_cos_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of sin_cos_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00664000;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_soft_load_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of soft_load_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_soft_load_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of soft_load_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006738C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_soft_save_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of soft_save_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_soft_save_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of soft_save_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00673880;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_add_hero_points_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_add_hero_points_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_add_hero_points_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_add_hero_points_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00679510;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_bank_stylepoints(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_bank_stylepoints\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_bank_stylepoints(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_bank_stylepoints\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794B0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_break_web(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_break_web\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_break_web(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_break_web\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794B0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_camera_add_shake_num_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_camera_add_shake_num_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_camera_add_shake_num_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_camera_add_shake_num_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006796F0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_camera_autocorrect_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_camera_autocorrect_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_camera_autocorrect_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_camera_autocorrect_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00679000;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_camera_clear_fixedstatic(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_camera_clear_fixedstatic\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_camera_clear_fixedstatic(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_camera_clear_fixedstatic\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006795D0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_camera_enable_combat_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_camera_enable_combat_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_camera_enable_combat_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_camera_enable_combat_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00679640;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_camera_enable_lookaround_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_camera_enable_lookaround_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_camera_enable_lookaround_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_camera_enable_lookaround_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006795F0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_camera_set_fixedstatic_vector3d_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_camera_set_fixedstatic_vector3d_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_camera_set_fixedstatic_vector3d_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_camera_set_fixedstatic_vector3d_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006795A0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_camera_set_follow_entity(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_camera_set_follow_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_camera_set_follow_entity(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_camera_set_follow_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006796A0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_camera_set_hero_underwater_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_camera_set_hero_underwater_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_camera_set_hero_underwater_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_camera_set_hero_underwater_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00679760;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_camera_set_interpolation_time_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_camera_set_interpolation_time_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_camera_set_interpolation_time_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_camera_set_interpolation_time_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_camera_set_lockon_min_distance_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_camera_set_lockon_min_distance_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_camera_set_lockon_min_distance_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_camera_set_lockon_min_distance_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_camera_set_lockon_y_offset_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_camera_set_lockon_y_offset_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_camera_set_lockon_y_offset_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_camera_set_lockon_y_offset_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_charged_jump(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_charged_jump\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_charged_jump(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_charged_jump\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00679390;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_enable_control_button_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_enable_control_button_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_enable_control_button_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_enable_control_button_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794D0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_enable_lockon_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_enable_lockon_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_enable_lockon_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_enable_lockon_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_engage_lockon_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_engage_lockon_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_engage_lockon_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_engage_lockon_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_engage_lockon_num_entity(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_engage_lockon_num_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_engage_lockon_num_entity(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_engage_lockon_num_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00679430;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_get_hero_points(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_get_hero_points\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_get_hero_points(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_get_hero_points\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794E0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_get_max_zip_length(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_get_max_zip_length\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_get_max_zip_length(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_get_max_zip_length\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794B0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_get_spidey_sense_level(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_get_spidey_sense_level\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_get_spidey_sense_level(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_get_spidey_sense_level\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00679400;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_is_crawling(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_is_crawling\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_is_crawling(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_is_crawling\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00679160;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_is_falling(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_is_falling\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_is_falling(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_is_falling\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006792F0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_is_jumping(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_is_jumping\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_is_jumping(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_is_jumping\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006792F0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_is_on_ceiling(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_is_on_ceiling\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_is_on_ceiling(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_is_on_ceiling\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00679200;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_is_on_ground(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_is_on_ground\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_is_on_ground(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_is_on_ground\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006792A0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_is_on_wall(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_is_on_wall\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_is_on_wall(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_is_on_wall\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006791B0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_is_running(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_is_running\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_is_running(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_is_running\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006792A0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_is_sprint_crawling(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_is_sprint_crawling\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_is_sprint_crawling(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_is_sprint_crawling\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00679390;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_is_sprinting(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_is_sprinting\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_is_sprinting(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_is_sprinting\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006792F0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_is_swinging(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_is_swinging\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_is_swinging(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_is_swinging\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00679250;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_is_wallsprinting(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_is_wallsprinting\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_is_wallsprinting(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_is_wallsprinting\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00679340;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_lock_spider_reflexes_off(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_lock_spider_reflexes_off\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_lock_spider_reflexes_off(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_lock_spider_reflexes_off\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794B0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_lock_spider_reflexes_on(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_lock_spider_reflexes_on\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_lock_spider_reflexes_on(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_lock_spider_reflexes_on\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794B0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_lockon_camera_engaged(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_lockon_camera_engaged\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_lockon_camera_engaged(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_lockon_camera_engaged\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794B0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_lockon_mode_engaged(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_lockon_mode_engaged\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_lockon_mode_engaged(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_lockon_mode_engaged\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794B0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_set_camera_target_entity(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_set_camera_target_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_set_camera_target_entity(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_set_camera_target_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00679460;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_set_desired_mode_num_vector3d_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_set_desired_mode_num_vector3d_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_set_desired_mode_num_vector3d_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_set_desired_mode_num_vector3d_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663390;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_set_health_beep_min_max_cooldown_time_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_set_health_beep_min_max_cooldown_time_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_set_health_beep_min_max_cooldown_time_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_set_health_beep_min_max_cooldown_time_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794D0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_set_health_beep_threshold_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_set_health_beep_threshold_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_set_health_beep_threshold_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_set_health_beep_threshold_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_set_hero_meter_empty_rate_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_set_hero_meter_empty_rate_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_set_hero_meter_empty_rate_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_set_hero_meter_empty_rate_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_set_max_height_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_set_max_height_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_set_max_height_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_set_max_height_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_set_max_zip_length_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_set_max_zip_length_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_set_max_zip_length_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_set_max_zip_length_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_set_min_height_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_set_min_height_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_set_min_height_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_set_min_height_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_set_spidey_sense_level_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_set_spidey_sense_level_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_set_spidey_sense_level_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_set_spidey_sense_level_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006793C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_set_swing_anchor_max_sticky_time_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_set_swing_anchor_max_sticky_time_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_set_swing_anchor_max_sticky_time_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_set_swing_anchor_max_sticky_time_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_subtract_hero_points_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_subtract_hero_points_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_subtract_hero_points_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_subtract_hero_points_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00679540;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_td_set_alternating_wall_run_occurrence_threshold_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_td_set_alternating_wall_run_occurrence_threshold_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_td_set_alternating_wall_run_occurrence_threshold_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_td_set_alternating_wall_run_occurrence_threshold_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_td_set_alternating_wall_run_time_threshold_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_td_set_alternating_wall_run_time_threshold_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_td_set_alternating_wall_run_time_threshold_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_td_set_alternating_wall_run_time_threshold_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_td_set_big_air_height_threshold_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_td_set_big_air_height_threshold_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_td_set_big_air_height_threshold_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_td_set_big_air_height_threshold_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_td_set_continuous_air_swings_threshold_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_td_set_continuous_air_swings_threshold_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_td_set_continuous_air_swings_threshold_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_td_set_continuous_air_swings_threshold_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_td_set_gain_altitude_height_threshold_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_td_set_gain_altitude_height_threshold_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_td_set_gain_altitude_height_threshold_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_td_set_gain_altitude_height_threshold_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_td_set_near_miss_trigger_radius_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_td_set_near_miss_trigger_radius_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_td_set_near_miss_trigger_radius_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_td_set_near_miss_trigger_radius_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_td_set_near_miss_velocity_threshold_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_td_set_near_miss_velocity_threshold_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_td_set_near_miss_velocity_threshold_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_td_set_near_miss_velocity_threshold_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_td_set_orbit_min_radius_threshold_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_td_set_orbit_min_radius_threshold_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_td_set_orbit_min_radius_threshold_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_td_set_orbit_min_radius_threshold_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_td_set_soft_landing_velocity_threshold_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_td_set_soft_landing_velocity_threshold_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_td_set_soft_landing_velocity_threshold_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_td_set_soft_landing_velocity_threshold_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_td_set_super_speed_speed_threshold_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_td_set_super_speed_speed_threshold_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_td_set_super_speed_speed_threshold_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_td_set_super_speed_speed_threshold_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_td_set_swinging_wall_run_time_threshold_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_td_set_swinging_wall_run_time_threshold_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_td_set_swinging_wall_run_time_threshold_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_td_set_swinging_wall_run_time_threshold_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_td_set_wall_sprint_time_threshold_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_td_set_wall_sprint_time_threshold_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_td_set_wall_sprint_time_threshold_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_td_set_wall_sprint_time_threshold_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_unlock_spider_reflexes(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_unlock_spider_reflexes\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_unlock_spider_reflexes(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_unlock_spider_reflexes\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794B0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spiderman_wait_add_threat_entity_str_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spiderman_wait_add_threat_entity_str_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spiderman_wait_add_threat_entity_str_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spiderman_wait_add_threat_entity_str_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006797C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_spidey_can_see_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of spidey_can_see_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_spidey_can_see_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of spidey_can_see_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00679030;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_sqrt_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of sqrt_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_sqrt_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of sqrt_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663F50;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_start_patrol_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of start_patrol_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_start_patrol_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of start_patrol_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00676E80;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_stop_all_sounds(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of stop_all_sounds\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_stop_all_sounds(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of stop_all_sounds\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0066EC20;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_stop_credits(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of stop_credits\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_stop_credits(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of stop_credits\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672690;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_stop_vibration(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of stop_vibration\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_stop_vibration(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of stop_vibration\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067A590;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_subtitle_num_num_num_num_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of subtitle_num_num_num_num_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_subtitle_num_num_num_num_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of subtitle_num_num_num_num_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00673900;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_swap_hero_costume_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of swap_hero_costume_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_swap_hero_costume_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of swap_hero_costume_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00676C50;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_text_width_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of text_width_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_text_width_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of text_width_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794B0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_timer_widget_get_count_up(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of timer_widget_get_count_up\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_timer_widget_get_count_up(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of timer_widget_get_count_up\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672940;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_timer_widget_get_time(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of timer_widget_get_time\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_timer_widget_get_time(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of timer_widget_get_time\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006728C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_timer_widget_set_count_up_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of timer_widget_set_count_up_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_timer_widget_set_count_up_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of timer_widget_set_count_up_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006728F0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_timer_widget_set_time_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of timer_widget_set_time_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_timer_widget_set_time_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of timer_widget_set_time_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672890;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_timer_widget_start(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of timer_widget_start\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_timer_widget_start(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of timer_widget_start\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672850;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_timer_widget_stop(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of timer_widget_stop\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_timer_widget_stop(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of timer_widget_stop\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672870;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_timer_widget_turn_off(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of timer_widget_turn_off\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_timer_widget_turn_off(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of timer_widget_turn_off\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672830;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_timer_widget_turn_on(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of timer_widget_turn_on\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_timer_widget_turn_on(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of timer_widget_turn_on\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672810;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_to_beam_entity(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of to_beam_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_to_beam_entity(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of to_beam_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067AE20;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_to_gun_entity(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of to_gun_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_to_gun_entity(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of to_gun_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00681950;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_to_item_entity(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of to_item_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_to_item_entity(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of to_item_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006819B0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_to_polytube_entity(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of to_polytube_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_to_polytube_entity(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of to_polytube_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006806D0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_to_switch_entity(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of to_switch_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_to_switch_entity(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of to_switch_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067F7F0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_trace_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of trace_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_trace_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of trace_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006794C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_trigger_is_valid_trigger(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of trigger_is_valid_trigger\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_trigger_is_valid_trigger(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of trigger_is_valid_trigger\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00671900;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_turn_off_boss_health(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of turn_off_boss_health\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_turn_off_boss_health(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of turn_off_boss_health\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672B00;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_turn_off_hero_health(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of turn_off_hero_health\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_turn_off_hero_health(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of turn_off_hero_health\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672B20;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_turn_off_mission_text(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of turn_off_mission_text\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_turn_off_mission_text(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of turn_off_mission_text\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00673060;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_turn_off_third_party_health(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of turn_off_third_party_health\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_turn_off_third_party_health(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of turn_off_third_party_health\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00672B40;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_turn_on_boss_health_num_entity(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of turn_on_boss_health_num_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_turn_on_boss_health_num_entity(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of turn_on_boss_health_num_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067FFF0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_turn_on_hero_health_num_entity(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of turn_on_hero_health_num_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_turn_on_hero_health_num_entity(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of turn_on_hero_health_num_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00680040;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_turn_on_third_party_health_num_entity(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of turn_on_third_party_health_num_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_turn_on_third_party_health_num_entity(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of turn_on_third_party_health_num_entity\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00680090;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_unload_script(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of unload_script\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_unload_script(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of unload_script\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x006762B0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_unlock_all_exterior_districts(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of unlock_all_exterior_districts\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_unlock_all_exterior_districts(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of unlock_all_exterior_districts\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00676C30;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_unlock_district_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of unlock_district_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_unlock_district_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of unlock_district_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00676A30;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_vibrate_controller_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of vibrate_controller_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_vibrate_controller_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of vibrate_controller_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067A550;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_vibrate_controller_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of vibrate_controller_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_vibrate_controller_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of vibrate_controller_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067A510;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_vibrate_controller_num_num_num_num_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of vibrate_controller_num_num_num_num_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_vibrate_controller_num_num_num_num_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of vibrate_controller_num_num_num_num_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067A4C0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_vo_delay_num_num_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of vo_delay_num_num_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_vo_delay_num_num_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of vo_delay_num_num_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663230;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_wait_animate_fog_color_vector3d_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of wait_animate_fog_color_vector3d_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_wait_animate_fog_color_vector3d_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of wait_animate_fog_color_vector3d_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067A100;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_wait_animate_fog_distance_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of wait_animate_fog_distance_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_wait_animate_fog_distance_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of wait_animate_fog_distance_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067A2A0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_wait_animate_fog_distances_num_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of wait_animate_fog_distances_num_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_wait_animate_fog_distances_num_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of wait_animate_fog_distances_num_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067A370;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_wait_change_blur_num_vector3d_num_num_num_num_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of wait_change_blur_num_vector3d_num_num_num_num_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_wait_change_blur_num_vector3d_num_num_num_num_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of wait_change_blur_num_vector3d_num_num_num_num_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00679BA0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_wait_change_spider_reflexes_blur_num_vector3d_num_num_num_num_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of wait_change_spider_reflexes_blur_num_vector3d_num_num_num_num_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_wait_change_spider_reflexes_blur_num_vector3d_num_num_num_num_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of wait_change_spider_reflexes_blur_num_vector3d_num_num_num_num_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00679E50;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_wait_for_streamer_to_reach_equilibrium(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of wait_for_streamer_to_reach_equilibrium\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_wait_for_streamer_to_reach_equilibrium(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of wait_for_streamer_to_reach_equilibrium\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00676B20;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_wait_fps_test_num_num_vector3d_vector3d(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of wait_fps_test_num_num_vector3d_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_wait_fps_test_num_num_vector3d_vector3d(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of wait_fps_test_num_num_vector3d_vector3d\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067A8F0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_wait_frame(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of wait_frame\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_wait_frame(void* _this, void* edx, void* arg, void* arg1)
{

        // printf("Calling action of wait_frame\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663110;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_wait_set_global_time_dilation_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of wait_set_global_time_dilation_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_wait_set_global_time_dilation_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of wait_set_global_time_dilation_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x0067A710;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_wait_set_zoom_num_num(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of wait_set_zoom_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_wait_set_zoom_num_num(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of wait_set_zoom_num_num\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00679AE0;
        return original(_this, edx, arg, arg1);
}

int __fastcall slf_deconstructor_write_to_file_str_str(void* _this, void* edx, void* arg)
{

        printf("Calling deconstructor of write_to_file_str_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*);
        original_ptr original = (original_ptr)0x0067ADE0;
        return original(_this, edx, arg);
}

int __fastcall slf_action_write_to_file_str_str(void* _this, void* edx, void* arg, void* arg1)
{

        printf("Calling action of write_to_file_str_str\n");

        typedef int(__fastcall * original_ptr)(void*, void*, void*, void*);
        original_ptr original = (original_ptr)0x00663620;
        return original(_this, edx, arg, arg1);
}





void hook_slf_vtables()
{

        


        
    hook_slf_vtable(slf_deconstructor_abs_delay_num, slf_action_abs_delay_num, static_cast<DWORD*>((void*)0x89a724));
    hook_slf_vtable(slf_deconstructor_acos_num, slf_action_acos_num, static_cast<DWORD*>((void*)0x89a91c));
    hook_slf_vtable(slf_deconstructor_add_2d_debug_str_vector3d_vector3d_num_str, slf_action_add_2d_debug_str_vector3d_vector3d_num_str, static_cast<DWORD*>((void*)0x89a860));
    hook_slf_vtable(slf_deconstructor_add_3d_debug_str_vector3d_vector3d_num_str, slf_action_add_3d_debug_str_vector3d_vector3d_num_str, static_cast<DWORD*>((void*)0x89a850));
    hook_slf_vtable(slf_deconstructor_add_civilian_info_vector3d_num_num_num, slf_action_add_civilian_info_vector3d_num_num_num, static_cast<DWORD*>((void*)0x89c5bc));
    hook_slf_vtable(slf_deconstructor_add_civilian_info_entity_entity_num_num_num, slf_action_add_civilian_info_entity_entity_num_num_num, static_cast<DWORD*>((void*)0x89c5cc));
    hook_slf_vtable(slf_deconstructor_add_debug_cyl_vector3d_vector3d_num, slf_action_add_debug_cyl_vector3d_vector3d_num, static_cast<DWORD*>((void*)0x89a774));
    hook_slf_vtable(slf_deconstructor_add_debug_cyl_vector3d_vector3d_num_vector3d_num, slf_action_add_debug_cyl_vector3d_vector3d_num_vector3d_num, static_cast<DWORD*>((void*)0x89a77c));
    hook_slf_vtable(slf_deconstructor_add_debug_line_vector3d_vector3d, slf_action_add_debug_line_vector3d_vector3d, static_cast<DWORD*>((void*)0x89a764));
    hook_slf_vtable(slf_deconstructor_add_debug_line_vector3d_vector3d_vector3d_num, slf_action_add_debug_line_vector3d_vector3d_vector3d_num, static_cast<DWORD*>((void*)0x89a76c));
    hook_slf_vtable(slf_deconstructor_add_debug_sphere_vector3d_num, slf_action_add_debug_sphere_vector3d_num, static_cast<DWORD*>((void*)0x89a754));
    hook_slf_vtable(slf_deconstructor_add_debug_sphere_vector3d_num_vector3d_num, slf_action_add_debug_sphere_vector3d_num_vector3d_num, static_cast<DWORD*>((void*)0x89a75c));
    hook_slf_vtable(slf_deconstructor_add_glass_house_str, slf_action_add_glass_house_str, static_cast<DWORD*>((void*)0x89a548));
    hook_slf_vtable(slf_deconstructor_add_glass_house_str_num, slf_action_add_glass_house_str_num, static_cast<DWORD*>((void*)0x89a550));
    hook_slf_vtable(slf_deconstructor_add_glass_house_str_num_vector3d, slf_action_add_glass_house_str_num_vector3d, static_cast<DWORD*>((void*)0x89a560));
    hook_slf_vtable(slf_deconstructor_add_glass_house_str_vector3d, slf_action_add_glass_house_str_vector3d, static_cast<DWORD*>((void*)0x89a558));
    hook_slf_vtable(slf_deconstructor_add_to_console_str, slf_action_add_to_console_str, static_cast<DWORD*>((void*)0x89a834));
    hook_slf_vtable(slf_deconstructor_add_traffic_model_num_str, slf_action_add_traffic_model_num_str, static_cast<DWORD*>((void*)0x89c5a4));
    hook_slf_vtable(slf_deconstructor_allow_suspend_thread_num, slf_action_allow_suspend_thread_num, static_cast<DWORD*>((void*)0x89a594));
    hook_slf_vtable(slf_deconstructor_angle_between_vector3d_vector3d, slf_action_angle_between_vector3d_vector3d, static_cast<DWORD*>((void*)0x89ba50));
    hook_slf_vtable(slf_deconstructor_apply_donut_damage_vector3d_num_num_num_num_num, slf_action_apply_donut_damage_vector3d_num_num_num_num_num, static_cast<DWORD*>((void*)0x89a804));
    hook_slf_vtable(slf_deconstructor_apply_radius_damage_vector3d_num_num_num_num, slf_action_apply_radius_damage_vector3d_num_num_num_num, static_cast<DWORD*>((void*)0x89a7fc));
    hook_slf_vtable(slf_deconstructor_apply_radius_subdue_vector3d_num_num_num_num, slf_action_apply_radius_subdue_vector3d_num_num_num_num, static_cast<DWORD*>((void*)0x89a80c));
    hook_slf_vtable(slf_deconstructor_assert_num_str, slf_action_assert_num_str, static_cast<DWORD*>((void*)0x89a518));
    hook_slf_vtable(slf_deconstructor_attach_decal_str_vector3d_num_vector3d_entity, slf_action_attach_decal_str_vector3d_num_vector3d_entity, static_cast<DWORD*>((void*)0x89a9fc));
    hook_slf_vtable(slf_deconstructor_begin_screen_recording_str_num, slf_action_begin_screen_recording_str_num, static_cast<DWORD*>((void*)0x89b7b0));
    hook_slf_vtable(slf_deconstructor_blackscreen_off_num, slf_action_blackscreen_off_num, static_cast<DWORD*>((void*)0x89bcac));
    hook_slf_vtable(slf_deconstructor_blackscreen_on_num, slf_action_blackscreen_on_num, static_cast<DWORD*>((void*)0x89bca0));
    hook_slf_vtable(slf_deconstructor_bring_up_dialog_box_num_num_elip, slf_action_bring_up_dialog_box_num_num_elip, static_cast<DWORD*>((void*)0x89bc28));
    hook_slf_vtable(slf_deconstructor_bring_up_dialog_box_debug_str_num_str, slf_action_bring_up_dialog_box_debug_str_num_str, static_cast<DWORD*>((void*)0x89bc38));
    hook_slf_vtable(slf_deconstructor_bring_up_dialog_box_title_num_num_num_elip, slf_action_bring_up_dialog_box_title_num_num_num_elip, static_cast<DWORD*>((void*)0x89bc30));
    hook_slf_vtable(slf_deconstructor_bring_up_medal_award_box_num, slf_action_bring_up_medal_award_box_num, static_cast<DWORD*>((void*)0x89bb10));
    hook_slf_vtable(slf_deconstructor_bring_up_race_announcer, slf_action_bring_up_race_announcer, static_cast<DWORD*>((void*)0x89bb08));
    hook_slf_vtable(slf_deconstructor_calc_launch_vector_vector3d_vector3d_num_entity, slf_action_calc_launch_vector_vector3d_vector3d_num_entity, static_cast<DWORD*>((void*)0x89a98c));
    hook_slf_vtable(slf_deconstructor_can_load_pack_str, slf_action_can_load_pack_str, static_cast<DWORD*>((void*)0x89c3f4));
    hook_slf_vtable(slf_deconstructor_chase_cam, slf_action_chase_cam, static_cast<DWORD*>((void*)0x89af04));
    hook_slf_vtable(slf_deconstructor_clear_all_grenades, slf_action_clear_all_grenades, static_cast<DWORD*>((void*)0x89a95c));
    hook_slf_vtable(slf_deconstructor_clear_civilians_within_radius_vector3d_num, slf_action_clear_civilians_within_radius_vector3d_num, static_cast<DWORD*>((void*)0x89c5e4));
    hook_slf_vtable(slf_deconstructor_clear_controls, slf_action_clear_controls, static_cast<DWORD*>((void*)0x89bd48));
    hook_slf_vtable(slf_deconstructor_clear_debug_all, slf_action_clear_debug_all, static_cast<DWORD*>((void*)0x89a79c));
    hook_slf_vtable(slf_deconstructor_clear_debug_cyls, slf_action_clear_debug_cyls, static_cast<DWORD*>((void*)0x89a794));
    hook_slf_vtable(slf_deconstructor_clear_debug_lines, slf_action_clear_debug_lines, static_cast<DWORD*>((void*)0x89a78c));
    hook_slf_vtable(slf_deconstructor_clear_debug_spheres, slf_action_clear_debug_spheres, static_cast<DWORD*>((void*)0x89a784));
    hook_slf_vtable(slf_deconstructor_clear_screen, slf_action_clear_screen, static_cast<DWORD*>((void*)0x89a944));
    hook_slf_vtable(slf_deconstructor_clear_traffic_within_radius_vector3d_num, slf_action_clear_traffic_within_radius_vector3d_num, static_cast<DWORD*>((void*)0x89c5dc));
    hook_slf_vtable(slf_deconstructor_col_check_vector3d_vector3d_num, slf_action_col_check_vector3d_vector3d_num, static_cast<DWORD*>((void*)0x89a868));
    hook_slf_vtable(slf_deconstructor_console_exec_str, slf_action_console_exec_str, static_cast<DWORD*>((void*)0x89a83c));
    hook_slf_vtable(slf_deconstructor_copy_vector3d_list_vector3d_list_vector3d_list, slf_action_copy_vector3d_list_vector3d_list_vector3d_list, static_cast<DWORD*>((void*)0x89bed4));
    hook_slf_vtable(slf_deconstructor_cos_num, slf_action_cos_num, static_cast<DWORD*>((void*)0x89a90c));
    hook_slf_vtable(slf_deconstructor_create_beam, slf_action_create_beam, static_cast<DWORD*>((void*)0x89abb4));
    hook_slf_vtable(slf_deconstructor_create_credits, slf_action_create_credits, static_cast<DWORD*>((void*)0x89bae8));
    hook_slf_vtable(slf_deconstructor_create_cut_scene_str, slf_action_create_cut_scene_str, static_cast<DWORD*>((void*)0x89b7c0));
    // hook_slf_vtable(slf_deconstructor_create_debug_menu_entry_str,slf_action_create_debug_menu_entry_str,static_cast<DWORD*>((void*)0x89c704));
    // hook_slf_vtable(slf_deconstructor_create_debug_menu_entry_str_str,slf_action_create_debug_menu_entry_str_str,static_cast<DWORD*>((void*)0x89c70c));
    hook_slf_vtable(slf_deconstructor_create_decal_str_vector3d_num_vector3d, slf_action_create_decal_str_vector3d_num_vector3d, static_cast<DWORD*>((void*)0x89a9f4));
    hook_slf_vtable(slf_deconstructor_create_entity_str_str, slf_action_create_entity_str_str, static_cast<DWORD*>((void*)0x89af14));
    hook_slf_vtable(slf_deconstructor_create_entity_in_hero_region_str, slf_action_create_entity_in_hero_region_str, static_cast<DWORD*>((void*)0x89af2c));
    hook_slf_vtable(slf_deconstructor_create_entity_list, slf_action_create_entity_list, static_cast<DWORD*>((void*)0x89bfcc));
    hook_slf_vtable(slf_deconstructor_create_entity_tracker_entity, slf_action_create_entity_tracker_entity, static_cast<DWORD*>((void*)0x89c594));
    hook_slf_vtable(slf_deconstructor_create_item_str, slf_action_create_item_str, static_cast<DWORD*>((void*)0x89b6b4));
    hook_slf_vtable(slf_deconstructor_create_line_info_vector3d_vector3d, slf_action_create_line_info_vector3d_vector3d, static_cast<DWORD*>((void*)0x89b708));
    hook_slf_vtable(slf_deconstructor_create_lofi_stereo_sound_inst_str, slf_action_create_lofi_stereo_sound_inst_str, static_cast<DWORD*>((void*)0x89b818));
    hook_slf_vtable(slf_deconstructor_create_num_list, slf_action_create_num_list, static_cast<DWORD*>((void*)0x89bf54));
    hook_slf_vtable(slf_deconstructor_create_pfx_str, slf_action_create_pfx_str, static_cast<DWORD*>((void*)0x89c904));
    hook_slf_vtable(slf_deconstructor_create_pfx_str_vector3d, slf_action_create_pfx_str_vector3d, static_cast<DWORD*>((void*)0x89c90c));
    hook_slf_vtable(slf_deconstructor_create_polytube, slf_action_create_polytube, static_cast<DWORD*>((void*)0x89c228));
    hook_slf_vtable(slf_deconstructor_create_polytube_str, slf_action_create_polytube_str, static_cast<DWORD*>((void*)0x89c230));
    // hook_slf_vtable(slf_deconstructor_create_progression_menu_entry_str_str,slf_action_create_progression_menu_entry_str_str,static_cast<DWORD*>((void*)0x89c714));
    hook_slf_vtable(slf_deconstructor_create_sound_inst, slf_action_create_sound_inst, static_cast<DWORD*>((void*)0x89b7f8));
    hook_slf_vtable(slf_deconstructor_create_sound_inst_str, slf_action_create_sound_inst_str, static_cast<DWORD*>((void*)0x89b800));
    hook_slf_vtable(slf_deconstructor_create_stompable_music_sound_inst_str, slf_action_create_stompable_music_sound_inst_str, static_cast<DWORD*>((void*)0x89b808));
    hook_slf_vtable(slf_deconstructor_create_str_list, slf_action_create_str_list, static_cast<DWORD*>((void*)0x89c044));
    hook_slf_vtable(slf_deconstructor_create_taunt_entry_entity_str_num, slf_action_create_taunt_entry_entity_str_num, static_cast<DWORD*>((void*)0x89c63c));
    hook_slf_vtable(slf_deconstructor_create_taunt_exchange_entity_entity_num_num_num_num_elip, slf_action_create_taunt_exchange_entity_entity_num_num_num_num_elip, static_cast<DWORD*>((void*)0x89c6b4));
    hook_slf_vtable(slf_deconstructor_create_taunt_exchange_list, slf_action_create_taunt_exchange_list, static_cast<DWORD*>((void*)0x89c0dc));
    hook_slf_vtable(slf_deconstructor_create_threat_assessment_meter, slf_action_create_threat_assessment_meter, static_cast<DWORD*>((void*)0x89c6cc));
    hook_slf_vtable(slf_deconstructor_create_time_limited_entity_str_num, slf_action_create_time_limited_entity_str_num, static_cast<DWORD*>((void*)0x89af3c));
    hook_slf_vtable(slf_deconstructor_create_trigger_entity_num, slf_action_create_trigger_entity_num, static_cast<DWORD*>((void*)0x89b970));
    hook_slf_vtable(slf_deconstructor_create_trigger_str_vector3d_num, slf_action_create_trigger_str_vector3d_num, static_cast<DWORD*>((void*)0x89b968));
    hook_slf_vtable(slf_deconstructor_create_trigger_vector3d_num, slf_action_create_trigger_vector3d_num, static_cast<DWORD*>((void*)0x89b960));
    hook_slf_vtable(slf_deconstructor_create_unstompable_script_cutscene_sound_inst_str, slf_action_create_unstompable_script_cutscene_sound_inst_str, static_cast<DWORD*>((void*)0x89b810));
    hook_slf_vtable(slf_deconstructor_create_vector3d_list, slf_action_create_vector3d_list, static_cast<DWORD*>((void*)0x89becc));
    hook_slf_vtable(slf_deconstructor_cross_vector3d_vector3d, slf_action_cross_vector3d_vector3d, static_cast<DWORD*>((void*)0x89ba38));
    hook_slf_vtable(slf_deconstructor_debug_breakpoint, slf_action_debug_breakpoint, static_cast<DWORD*>((void*)0x89a510));
    hook_slf_vtable(slf_deconstructor_debug_print_num_str, slf_action_debug_print_num_str, static_cast<DWORD*>((void*)0x89a528));
    hook_slf_vtable(slf_deconstructor_debug_print_num_vector3d_str, slf_action_debug_print_num_vector3d_str, static_cast<DWORD*>((void*)0x89a530));
    hook_slf_vtable(slf_deconstructor_debug_print_str, slf_action_debug_print_str, static_cast<DWORD*>((void*)0x89a520));
    hook_slf_vtable(slf_deconstructor_debug_print_set_background_color_vector3d, slf_action_debug_print_set_background_color_vector3d, static_cast<DWORD*>((void*)0x89a538));
    hook_slf_vtable(slf_deconstructor_delay_num, slf_action_delay_num, static_cast<DWORD*>((void*)0x89a71c));
    hook_slf_vtable(slf_deconstructor_destroy_credits, slf_action_destroy_credits, static_cast<DWORD*>((void*)0x89baf0));
    // hook_slf_vtable(slf_deconstructor_destroy_debug_menu_entry_debug_menu_entry,slf_action_destroy_debug_menu_entry_debug_menu_entry,static_cast<DWORD*>((void*)0x89c71c));
    hook_slf_vtable(slf_deconstructor_destroy_entity_entity, slf_action_destroy_entity_entity, static_cast<DWORD*>((void*)0x89af34));
    hook_slf_vtable(slf_deconstructor_destroy_entity_list_entity_list, slf_action_destroy_entity_list_entity_list, static_cast<DWORD*>((void*)0x89bfd4));
    hook_slf_vtable(slf_deconstructor_destroy_entity_tracker_entity_tracker, slf_action_destroy_entity_tracker_entity_tracker, static_cast<DWORD*>((void*)0x89c59c));
    hook_slf_vtable(slf_deconstructor_destroy_line_info_line_info, slf_action_destroy_line_info_line_info, static_cast<DWORD*>((void*)0x89b710));
    hook_slf_vtable(slf_deconstructor_destroy_num_list_num_list, slf_action_destroy_num_list_num_list, static_cast<DWORD*>((void*)0x89bf5c));
    hook_slf_vtable(slf_deconstructor_destroy_pfx_pfx, slf_action_destroy_pfx_pfx, static_cast<DWORD*>((void*)0x89c914));
    hook_slf_vtable(slf_deconstructor_destroy_str_list_str_list, slf_action_destroy_str_list_str_list, static_cast<DWORD*>((void*)0x89c04c));
    hook_slf_vtable(slf_deconstructor_destroy_taunt_entry_taunt_entry, slf_action_destroy_taunt_entry_taunt_entry, static_cast<DWORD*>((void*)0x89c644));
    hook_slf_vtable(slf_deconstructor_destroy_taunt_exchange_taunt_exchange, slf_action_destroy_taunt_exchange_taunt_exchange, static_cast<DWORD*>((void*)0x89c6bc));
    hook_slf_vtable(slf_deconstructor_destroy_taunt_exchange_list_taunt_exchange_list, slf_action_destroy_taunt_exchange_list_taunt_exchange_list, static_cast<DWORD*>((void*)0x89c0e4));
    hook_slf_vtable(slf_deconstructor_destroy_threat_assessment_meter_tam, slf_action_destroy_threat_assessment_meter_tam, static_cast<DWORD*>((void*)0x89c6d4));
    hook_slf_vtable(slf_deconstructor_destroy_trigger_trigger, slf_action_destroy_trigger_trigger, static_cast<DWORD*>((void*)0x89b978));
    hook_slf_vtable(slf_deconstructor_destroy_vector3d_list_vector3d_list, slf_action_destroy_vector3d_list_vector3d_list, static_cast<DWORD*>((void*)0x89bedc));
    hook_slf_vtable(slf_deconstructor_dilated_delay_num, slf_action_dilated_delay_num, static_cast<DWORD*>((void*)0x89a72c));
    hook_slf_vtable(slf_deconstructor_disable_marky_cam_num, slf_action_disable_marky_cam_num, static_cast<DWORD*>((void*)0x89a5f4));
    hook_slf_vtable(slf_deconstructor_disable_nearby_occlusion_only_obb_vector3d, slf_action_disable_nearby_occlusion_only_obb_vector3d, static_cast<DWORD*>((void*)0x89a5e4));
    hook_slf_vtable(slf_deconstructor_disable_player_shadows, slf_action_disable_player_shadows, static_cast<DWORD*>((void*)0x89a614));
    hook_slf_vtable(slf_deconstructor_disable_subtitles, slf_action_disable_subtitles, static_cast<DWORD*>((void*)0x89a93c));
    hook_slf_vtable(slf_deconstructor_disable_vibrator, slf_action_disable_vibrator, static_cast<DWORD*>((void*)0x89a7dc));
    hook_slf_vtable(slf_deconstructor_disable_zoom_map_num, slf_action_disable_zoom_map_num, static_cast<DWORD*>((void*)0x89bbf0));
    hook_slf_vtable(slf_deconstructor_distance3d_vector3d_vector3d, slf_action_distance3d_vector3d_vector3d, static_cast<DWORD*>((void*)0x89ba48));
    hook_slf_vtable(slf_deconstructor_distance_chase_widget_set_pos_num, slf_action_distance_chase_widget_set_pos_num, static_cast<DWORD*>((void*)0x89bb88));
    hook_slf_vtable(slf_deconstructor_distance_chase_widget_turn_off, slf_action_distance_chase_widget_turn_off, static_cast<DWORD*>((void*)0x89bb80));
    hook_slf_vtable(slf_deconstructor_distance_chase_widget_turn_on_num_num, slf_action_distance_chase_widget_turn_on_num_num, static_cast<DWORD*>((void*)0x89bb78));
    hook_slf_vtable(slf_deconstructor_distance_race_widget_set_boss_pos_num, slf_action_distance_race_widget_set_boss_pos_num, static_cast<DWORD*>((void*)0x89bba8));
    hook_slf_vtable(slf_deconstructor_distance_race_widget_set_hero_pos_num, slf_action_distance_race_widget_set_hero_pos_num, static_cast<DWORD*>((void*)0x89bba0));
    hook_slf_vtable(slf_deconstructor_distance_race_widget_set_types_num_num, slf_action_distance_race_widget_set_types_num_num, static_cast<DWORD*>((void*)0x89bbb0));
    hook_slf_vtable(slf_deconstructor_distance_race_widget_turn_off, slf_action_distance_race_widget_turn_off, static_cast<DWORD*>((void*)0x89bb98));
    hook_slf_vtable(slf_deconstructor_distance_race_widget_turn_on, slf_action_distance_race_widget_turn_on, static_cast<DWORD*>((void*)0x89bb90));
    hook_slf_vtable(slf_deconstructor_district_id_str, slf_action_district_id_str, static_cast<DWORD*>((void*)0x89c47c));
    hook_slf_vtable(slf_deconstructor_district_name_num, slf_action_district_name_num, static_cast<DWORD*>((void*)0x89c484));
    hook_slf_vtable(slf_deconstructor_dot_vector3d_vector3d, slf_action_dot_vector3d_vector3d, static_cast<DWORD*>((void*)0x89ba30));
    hook_slf_vtable(slf_deconstructor_dump_searchable_region_list_str, slf_action_dump_searchable_region_list_str, static_cast<DWORD*>((void*)0x89a994));
    hook_slf_vtable(slf_deconstructor_enable_ai_num, slf_action_enable_ai_num, static_cast<DWORD*>((void*)0x89a6cc));
    hook_slf_vtable(slf_deconstructor_enable_civilians_num, slf_action_enable_civilians_num, static_cast<DWORD*>((void*)0x89c5ec));
    hook_slf_vtable(slf_deconstructor_enable_controls_num, slf_action_enable_controls_num, static_cast<DWORD*>((void*)0x89bd40));
    hook_slf_vtable(slf_deconstructor_enable_entity_fading_num, slf_action_enable_entity_fading_num, static_cast<DWORD*>((void*)0x89b05c));
    hook_slf_vtable(slf_deconstructor_enable_interface_num, slf_action_enable_interface_num, static_cast<DWORD*>((void*)0x89a6c4));
    hook_slf_vtable(slf_deconstructor_enable_marky_cam_num, slf_action_enable_marky_cam_num, static_cast<DWORD*>((void*)0x89a5bc));
    hook_slf_vtable(slf_deconstructor_enable_mini_map_num, slf_action_enable_mini_map_num, static_cast<DWORD*>((void*)0x89bbe8));
    hook_slf_vtable(slf_deconstructor_enable_nearby_occlusion_only_obb_vector3d, slf_action_enable_nearby_occlusion_only_obb_vector3d, static_cast<DWORD*>((void*)0x89a5dc));
    hook_slf_vtable(slf_deconstructor_enable_obb_vector3d_num, slf_action_enable_obb_vector3d_num, static_cast<DWORD*>((void*)0x89a588));
    hook_slf_vtable(slf_deconstructor_enable_pause_num, slf_action_enable_pause_num, static_cast<DWORD*>((void*)0x89a6ac));
    hook_slf_vtable(slf_deconstructor_enable_physics_num, slf_action_enable_physics_num, static_cast<DWORD*>((void*)0x89a6dc));
    hook_slf_vtable(slf_deconstructor_enable_player_shadows, slf_action_enable_player_shadows, static_cast<DWORD*>((void*)0x89a61c));
    hook_slf_vtable(slf_deconstructor_enable_pois_num, slf_action_enable_pois_num, static_cast<DWORD*>((void*)0x89a6ec));
    hook_slf_vtable(slf_deconstructor_enable_quad_path_connector_district_num_district_num_num, slf_action_enable_quad_path_connector_district_num_district_num_num, static_cast<DWORD*>((void*)0x89a578));
    hook_slf_vtable(slf_deconstructor_enable_subtitles, slf_action_enable_subtitles, static_cast<DWORD*>((void*)0x89a934));
    hook_slf_vtable(slf_deconstructor_enable_tokens_of_type_num_num, slf_action_enable_tokens_of_type_num_num, static_cast<DWORD*>((void*)0x89b54c));
    hook_slf_vtable(slf_deconstructor_enable_traffic_num, slf_action_enable_traffic_num, static_cast<DWORD*>((void*)0x89c5fc));
    hook_slf_vtable(slf_deconstructor_enable_user_camera_num, slf_action_enable_user_camera_num, static_cast<DWORD*>((void*)0x89a5d4));
    hook_slf_vtable(slf_deconstructor_enable_vibrator, slf_action_enable_vibrator, static_cast<DWORD*>((void*)0x89a7e4));
    hook_slf_vtable(slf_deconstructor_end_current_patrol, slf_action_end_current_patrol, static_cast<DWORD*>((void*)0x89c4fc));
    hook_slf_vtable(slf_deconstructor_end_cut_scenes, slf_action_end_cut_scenes, static_cast<DWORD*>((void*)0x89b7c8));
    hook_slf_vtable(slf_deconstructor_end_screen_recording, slf_action_end_screen_recording, static_cast<DWORD*>((void*)0x89b7b8));
    hook_slf_vtable(slf_deconstructor_entity_col_check_entity_entity, slf_action_entity_col_check_entity_entity, static_cast<DWORD*>((void*)0x89a88c));
    hook_slf_vtable(slf_deconstructor_entity_get_entity_tracker_entity, slf_action_entity_get_entity_tracker_entity, static_cast<DWORD*>((void*)0x89b034));
    hook_slf_vtable(slf_deconstructor_entity_has_entity_tracker_entity, slf_action_entity_has_entity_tracker_entity, static_cast<DWORD*>((void*)0x89b02c));
    hook_slf_vtable(slf_deconstructor_exit_water_entity, slf_action_exit_water_entity, static_cast<DWORD*>((void*)0x89a604));
    hook_slf_vtable(slf_deconstructor_find_closest_point_on_a_path_to_point_vector3d, slf_action_find_closest_point_on_a_path_to_point_vector3d, static_cast<DWORD*>((void*)0x89a570));
    hook_slf_vtable(slf_deconstructor_find_district_for_point_vector3d, slf_action_find_district_for_point_vector3d, static_cast<DWORD*>((void*)0x89a81c));
    hook_slf_vtable(slf_deconstructor_find_entities_in_radius_entity_list_vector3d_num_num, slf_action_find_entities_in_radius_entity_list_vector3d_num_num, static_cast<DWORD*>((void*)0x89b46c));
    hook_slf_vtable(slf_deconstructor_find_entity_str, slf_action_find_entity_str, static_cast<DWORD*>((void*)0x89af1c));
    hook_slf_vtable(slf_deconstructor_find_innermost_district_vector3d, slf_action_find_innermost_district_vector3d, static_cast<DWORD*>((void*)0x89a824));
    hook_slf_vtable(slf_deconstructor_find_outermost_district_vector3d, slf_action_find_outermost_district_vector3d, static_cast<DWORD*>((void*)0x89a82c));
    hook_slf_vtable(slf_deconstructor_find_trigger_entity, slf_action_find_trigger_entity, static_cast<DWORD*>((void*)0x89b950));
    hook_slf_vtable(slf_deconstructor_find_trigger_str, slf_action_find_trigger_str, static_cast<DWORD*>((void*)0x89b948));
    hook_slf_vtable(slf_deconstructor_find_trigger_in_district_district_str, slf_action_find_trigger_in_district_district_str, static_cast<DWORD*>((void*)0x89b958));
    hook_slf_vtable(slf_deconstructor_float_random_num, slf_action_float_random_num, static_cast<DWORD*>((void*)0x89a74c));
    hook_slf_vtable(slf_deconstructor_force_mission_district_str_num, slf_action_force_mission_district_str_num, static_cast<DWORD*>((void*)0x89c3b4));
    hook_slf_vtable(slf_deconstructor_force_streamer_refresh, slf_action_force_streamer_refresh, static_cast<DWORD*>((void*)0x89c4ac));
    hook_slf_vtable(slf_deconstructor_format_time_string_num, slf_action_format_time_string_num, static_cast<DWORD*>((void*)0x89bc74));
    hook_slf_vtable(slf_deconstructor_freeze_hero_num, slf_action_freeze_hero_num, static_cast<DWORD*>((void*)0x89a5fc));
    hook_slf_vtable(slf_deconstructor_game_ini_get_flag_str, slf_action_game_ini_get_flag_str, static_cast<DWORD*>((void*)0x89a97c));
    hook_slf_vtable(slf_deconstructor_game_time_advance_num_num, slf_action_game_time_advance_num_num, static_cast<DWORD*>((void*)0x89c4dc));
    hook_slf_vtable(slf_deconstructor_get_all_execs_thread_count_str, slf_action_get_all_execs_thread_count_str, static_cast<DWORD*>((void*)0x89a9ec));
    hook_slf_vtable(slf_deconstructor_get_all_instances_thread_count_str, slf_action_get_all_instances_thread_count_str, static_cast<DWORD*>((void*)0x89a9e4));
    hook_slf_vtable(slf_deconstructor_get_attacker_entity, slf_action_get_attacker_entity, static_cast<DWORD*>((void*)0x89aa24));
    hook_slf_vtable(slf_deconstructor_get_attacker_member, slf_action_get_attacker_member, static_cast<DWORD*>((void*)0x89aa2c));
    hook_slf_vtable(slf_deconstructor_get_available_stack_size, slf_action_get_available_stack_size, static_cast<DWORD*>((void*)0x89c4ec));
    hook_slf_vtable(slf_deconstructor_get_character_packname_list, slf_action_get_character_packname_list, static_cast<DWORD*>((void*)0x89c354));
    hook_slf_vtable(slf_deconstructor_get_closest_point_on_lane_with_facing_num_vector3d_vector3d_list, slf_action_get_closest_point_on_lane_with_facing_num_vector3d_vector3d_list, static_cast<DWORD*>((void*)0x89c61c));
    hook_slf_vtable(slf_deconstructor_get_col_hit_ent, slf_action_get_col_hit_ent, static_cast<DWORD*>((void*)0x89a884));
    hook_slf_vtable(slf_deconstructor_get_col_hit_norm, slf_action_get_col_hit_norm, static_cast<DWORD*>((void*)0x89a87c));
    hook_slf_vtable(slf_deconstructor_get_col_hit_pos, slf_action_get_col_hit_pos, static_cast<DWORD*>((void*)0x89a874));
    hook_slf_vtable(slf_deconstructor_get_control_state_num, slf_action_get_control_state_num, static_cast<DWORD*>((void*)0x89a7f4));
    hook_slf_vtable(slf_deconstructor_get_control_trigger_num, slf_action_get_control_trigger_num, static_cast<DWORD*>((void*)0x89a7ec));
    hook_slf_vtable(slf_deconstructor_get_current_instance_thread_count_str, slf_action_get_current_instance_thread_count_str, static_cast<DWORD*>((void*)0x89a9dc));
    hook_slf_vtable(slf_deconstructor_get_current_view_cam_pos, slf_action_get_current_view_cam_pos, static_cast<DWORD*>((void*)0x89a5b4));
    hook_slf_vtable(slf_deconstructor_get_current_view_cam_x_facing, slf_action_get_current_view_cam_x_facing, static_cast<DWORD*>((void*)0x89a59c));
    hook_slf_vtable(slf_deconstructor_get_current_view_cam_y_facing, slf_action_get_current_view_cam_y_facing, static_cast<DWORD*>((void*)0x89a5a4));
    hook_slf_vtable(slf_deconstructor_get_current_view_cam_z_facing, slf_action_get_current_view_cam_z_facing, static_cast<DWORD*>((void*)0x89a5ac));
    hook_slf_vtable(slf_deconstructor_get_fog_color, slf_action_get_fog_color, static_cast<DWORD*>((void*)0x89a8f4));
    hook_slf_vtable(slf_deconstructor_get_fog_distance, slf_action_get_fog_distance, static_cast<DWORD*>((void*)0x89a8fc));
    hook_slf_vtable(slf_deconstructor_get_game_info_num_str, slf_action_get_game_info_num_str, static_cast<DWORD*>((void*)0x89a8c4));
    hook_slf_vtable(slf_deconstructor_get_game_info_str_str, slf_action_get_game_info_str_str, static_cast<DWORD*>((void*)0x89a8d4));
    hook_slf_vtable(slf_deconstructor_get_glam_cam_num, slf_action_get_glam_cam_num, static_cast<DWORD*>((void*)0x89b780));
    hook_slf_vtable(slf_deconstructor_get_global_time_dilation, slf_action_get_global_time_dilation, static_cast<DWORD*>((void*)0x89a894));
    hook_slf_vtable(slf_deconstructor_get_ini_flag_str, slf_action_get_ini_flag_str, static_cast<DWORD*>((void*)0x89a94c));
    hook_slf_vtable(slf_deconstructor_get_ini_num_str, slf_action_get_ini_num_str, static_cast<DWORD*>((void*)0x89a954));
    hook_slf_vtable(slf_deconstructor_get_int_num_num, slf_action_get_int_num_num, static_cast<DWORD*>((void*)0x89a92c));
    hook_slf_vtable(slf_deconstructor_get_mission_camera_marker_num, slf_action_get_mission_camera_marker_num, static_cast<DWORD*>((void*)0x89c414));
    hook_slf_vtable(slf_deconstructor_get_mission_camera_transform_marker_num, slf_action_get_mission_camera_transform_marker_num, static_cast<DWORD*>((void*)0x89c454));
    hook_slf_vtable(slf_deconstructor_get_mission_entity, slf_action_get_mission_entity, static_cast<DWORD*>((void*)0x89c38c));
    hook_slf_vtable(slf_deconstructor_get_mission_key_posfacing3d, slf_action_get_mission_key_posfacing3d, static_cast<DWORD*>((void*)0x89c36c));
    hook_slf_vtable(slf_deconstructor_get_mission_key_position, slf_action_get_mission_key_position, static_cast<DWORD*>((void*)0x89c364));
    hook_slf_vtable(slf_deconstructor_get_mission_marker_num, slf_action_get_mission_marker_num, static_cast<DWORD*>((void*)0x89c40c));
    hook_slf_vtable(slf_deconstructor_get_mission_nums, slf_action_get_mission_nums, static_cast<DWORD*>((void*)0x89c3ac));
    hook_slf_vtable(slf_deconstructor_get_mission_patrol_waypoint, slf_action_get_mission_patrol_waypoint, static_cast<DWORD*>((void*)0x89c384));
    hook_slf_vtable(slf_deconstructor_get_mission_positions, slf_action_get_mission_positions, static_cast<DWORD*>((void*)0x89c39c));
    hook_slf_vtable(slf_deconstructor_get_mission_strings, slf_action_get_mission_strings, static_cast<DWORD*>((void*)0x89c3a4));
    hook_slf_vtable(slf_deconstructor_get_mission_transform_marker_num, slf_action_get_mission_transform_marker_num, static_cast<DWORD*>((void*)0x89c42c));
    hook_slf_vtable(slf_deconstructor_get_mission_trigger, slf_action_get_mission_trigger, static_cast<DWORD*>((void*)0x89c394));
    hook_slf_vtable(slf_deconstructor_get_missions_key_position_by_index_district_str_num, slf_action_get_missions_key_position_by_index_district_str_num, static_cast<DWORD*>((void*)0x89c3bc));
    hook_slf_vtable(slf_deconstructor_get_missions_nums_by_index_district_str_num_num_list, slf_action_get_missions_nums_by_index_district_str_num_num_list, static_cast<DWORD*>((void*)0x89c3cc));
    hook_slf_vtable(slf_deconstructor_get_missions_patrol_waypoint_by_index_district_str_num, slf_action_get_missions_patrol_waypoint_by_index_district_str_num, static_cast<DWORD*>((void*)0x89c3c4));
    hook_slf_vtable(slf_deconstructor_get_neighborhood_name_num, slf_action_get_neighborhood_name_num, static_cast<DWORD*>((void*)0x89c54c));
    hook_slf_vtable(slf_deconstructor_get_num_free_slots_str, slf_action_get_num_free_slots_str, static_cast<DWORD*>((void*)0x89c3e4));
    hook_slf_vtable(slf_deconstructor_get_num_mission_transform_marker, slf_action_get_num_mission_transform_marker, static_cast<DWORD*>((void*)0x89c434));
    hook_slf_vtable(slf_deconstructor_get_pack_group_str, slf_action_get_pack_group_str, static_cast<DWORD*>((void*)0x89c3ec));
    hook_slf_vtable(slf_deconstructor_get_pack_size_str, slf_action_get_pack_size_str, static_cast<DWORD*>((void*)0x89c4e4));
    hook_slf_vtable(slf_deconstructor_get_patrol_difficulty_str, slf_action_get_patrol_difficulty_str, static_cast<DWORD*>((void*)0x89c534));
    hook_slf_vtable(slf_deconstructor_get_patrol_node_position_by_index_str_num, slf_action_get_patrol_node_position_by_index_str_num, static_cast<DWORD*>((void*)0x89c52c));
    hook_slf_vtable(slf_deconstructor_get_patrol_start_position_str, slf_action_get_patrol_start_position_str, static_cast<DWORD*>((void*)0x89c524));
    hook_slf_vtable(slf_deconstructor_get_patrol_unlock_threshold_str, slf_action_get_patrol_unlock_threshold_str, static_cast<DWORD*>((void*)0x89c53c));
    hook_slf_vtable(slf_deconstructor_get_platform, slf_action_get_platform, static_cast<DWORD*>((void*)0x89a508));
    hook_slf_vtable(slf_deconstructor_get_render_opt_num_str, slf_action_get_render_opt_num_str, static_cast<DWORD*>((void*)0x89a8e4));
    hook_slf_vtable(slf_deconstructor_get_spider_reflexes_spiderman_time_dilation, slf_action_get_spider_reflexes_spiderman_time_dilation, static_cast<DWORD*>((void*)0x89cac4));
    hook_slf_vtable(slf_deconstructor_get_spider_reflexes_world_time_dilation, slf_action_get_spider_reflexes_world_time_dilation, static_cast<DWORD*>((void*)0x89cad4));
    hook_slf_vtable(slf_deconstructor_get_time_inc, slf_action_get_time_inc, static_cast<DWORD*>((void*)0x89a7a4));
    hook_slf_vtable(slf_deconstructor_get_time_of_day, slf_action_get_time_of_day, static_cast<DWORD*>((void*)0x89a974));
    hook_slf_vtable(slf_deconstructor_get_time_of_day_rate, slf_action_get_time_of_day_rate, static_cast<DWORD*>((void*)0x89a96c));
    hook_slf_vtable(slf_deconstructor_get_token_index_from_id_num_num, slf_action_get_token_index_from_id_num_num, static_cast<DWORD*>((void*)0x89b554));
    hook_slf_vtable(slf_deconstructor_get_traffic_spawn_point_near_camera_vector3d_list, slf_action_get_traffic_spawn_point_near_camera_vector3d_list, static_cast<DWORD*>((void*)0x89aa98));
    hook_slf_vtable(slf_deconstructor_greater_than_or_equal_rounded_num_num, slf_action_greater_than_or_equal_rounded_num_num, static_cast<DWORD*>((void*)0x89bc90));
    hook_slf_vtable(slf_deconstructor_hard_break, slf_action_hard_break, static_cast<DWORD*>((void*)0x89aa1c));
    hook_slf_vtable(slf_deconstructor_has_substring_str_str, slf_action_has_substring_str_str, static_cast<DWORD*>((void*)0x89a580));
    hook_slf_vtable(slf_deconstructor_hero, slf_action_hero, static_cast<DWORD*>((void*)0x89aed4));
    hook_slf_vtable(slf_deconstructor_hero_exists, slf_action_hero_exists, static_cast<DWORD*>((void*)0x89aedc));
    hook_slf_vtable(slf_deconstructor_hero_type, slf_action_hero_type, static_cast<DWORD*>((void*)0x89aee4));
    hook_slf_vtable(slf_deconstructor_hide_controller_gauge, slf_action_hide_controller_gauge, static_cast<DWORD*>((void*)0x89bb20));
    hook_slf_vtable(slf_deconstructor_initialize_encounter_object, slf_action_initialize_encounter_object, static_cast<DWORD*>((void*)0x89aa0c));
    hook_slf_vtable(slf_deconstructor_initialize_encounter_objects, slf_action_initialize_encounter_objects, static_cast<DWORD*>((void*)0x89aa04));
    hook_slf_vtable(slf_deconstructor_insert_pack_str, slf_action_insert_pack_str, static_cast<DWORD*>((void*)0x89c3d4));
    hook_slf_vtable(slf_deconstructor_invoke_pause_menu_unlockables, slf_action_invoke_pause_menu_unlockables, static_cast<DWORD*>((void*)0x89bc98));
    hook_slf_vtable(slf_deconstructor_is_ai_enabled, slf_action_is_ai_enabled, static_cast<DWORD*>((void*)0x89a6d4));
    hook_slf_vtable(slf_deconstructor_is_cut_scene_playing, slf_action_is_cut_scene_playing, static_cast<DWORD*>((void*)0x89b7d0));
    hook_slf_vtable(slf_deconstructor_is_district_loaded_num, slf_action_is_district_loaded_num, static_cast<DWORD*>((void*)0x89c49c));
    hook_slf_vtable(slf_deconstructor_is_hero_frozen, slf_action_is_hero_frozen, static_cast<DWORD*>((void*)0x89a60c));
    hook_slf_vtable(slf_deconstructor_is_hero_peter_parker, slf_action_is_hero_peter_parker, static_cast<DWORD*>((void*)0x89aefc));
    hook_slf_vtable(slf_deconstructor_is_hero_spidey, slf_action_is_hero_spidey, static_cast<DWORD*>((void*)0x89aeec));
    hook_slf_vtable(slf_deconstructor_is_hero_venom, slf_action_is_hero_venom, static_cast<DWORD*>((void*)0x89aef4));
    hook_slf_vtable(slf_deconstructor_is_marky_cam_enabled, slf_action_is_marky_cam_enabled, static_cast<DWORD*>((void*)0x89a5c4));
    hook_slf_vtable(slf_deconstructor_is_mission_active, slf_action_is_mission_active, static_cast<DWORD*>((void*)0x89c50c));
    hook_slf_vtable(slf_deconstructor_is_mission_loading, slf_action_is_mission_loading, static_cast<DWORD*>((void*)0x89c514));
    hook_slf_vtable(slf_deconstructor_is_pack_available_str, slf_action_is_pack_available_str, static_cast<DWORD*>((void*)0x89c404));
    hook_slf_vtable(slf_deconstructor_is_pack_loaded_str, slf_action_is_pack_loaded_str, static_cast<DWORD*>((void*)0x89c3fc));
    hook_slf_vtable(slf_deconstructor_is_pack_pushed_str, slf_action_is_pack_pushed_str, static_cast<DWORD*>((void*)0x89c34c));
    hook_slf_vtable(slf_deconstructor_is_path_graph_inside_glass_house_str, slf_action_is_path_graph_inside_glass_house_str, static_cast<DWORD*>((void*)0x89aaa0));
    hook_slf_vtable(slf_deconstructor_is_patrol_active, slf_action_is_patrol_active, static_cast<DWORD*>((void*)0x89c504));
    hook_slf_vtable(slf_deconstructor_is_patrol_node_empty_num, slf_action_is_patrol_node_empty_num, static_cast<DWORD*>((void*)0x89c544));
    hook_slf_vtable(slf_deconstructor_is_paused, slf_action_is_paused, static_cast<DWORD*>((void*)0x89a6b4));
    hook_slf_vtable(slf_deconstructor_is_physics_enabled, slf_action_is_physics_enabled, static_cast<DWORD*>((void*)0x89a6e4));
    hook_slf_vtable(slf_deconstructor_is_point_inside_glass_house_vector3d, slf_action_is_point_inside_glass_house_vector3d, static_cast<DWORD*>((void*)0x89a540));
    hook_slf_vtable(slf_deconstructor_is_point_under_water_vector3d, slf_action_is_point_under_water_vector3d, static_cast<DWORD*>((void*)0x89aa3c));
    hook_slf_vtable(slf_deconstructor_is_user_camera_enabled, slf_action_is_user_camera_enabled, static_cast<DWORD*>((void*)0x89a5cc));
    hook_slf_vtable(slf_deconstructor_load_anim_str, slf_action_load_anim_str, static_cast<DWORD*>((void*)0x89aaf0));
    hook_slf_vtable(slf_deconstructor_load_level_str_vector3d, slf_action_load_level_str_vector3d, static_cast<DWORD*>((void*)0x89a8ac));
    hook_slf_vtable(slf_deconstructor_lock_all_districts, slf_action_lock_all_districts, static_cast<DWORD*>((void*)0x89c4c4));
    hook_slf_vtable(slf_deconstructor_lock_district_num, slf_action_lock_district_num, static_cast<DWORD*>((void*)0x89c494));
    hook_slf_vtable(slf_deconstructor_lock_mission_manager_num, slf_action_lock_mission_manager_num, static_cast<DWORD*>((void*)0x89c51c));
    hook_slf_vtable(slf_deconstructor_los_check_vector3d_vector3d, slf_action_los_check_vector3d_vector3d, static_cast<DWORD*>((void*)0x89a814));
    hook_slf_vtable(slf_deconstructor_lower_hotpursuit_indicator_level, slf_action_lower_hotpursuit_indicator_level, static_cast<DWORD*>((void*)0x89bae0));
    hook_slf_vtable(slf_deconstructor_malor_vector3d_num, slf_action_malor_vector3d_num, static_cast<DWORD*>((void*)0x89a984));
    hook_slf_vtable(slf_deconstructor_normal_vector3d, slf_action_normal_vector3d, static_cast<DWORD*>((void*)0x89ba40));
    hook_slf_vtable(slf_deconstructor_pause_game_num, slf_action_pause_game_num, static_cast<DWORD*>((void*)0x89a6bc));
    hook_slf_vtable(slf_deconstructor_play_credits, slf_action_play_credits, static_cast<DWORD*>((void*)0x89baf8));
    hook_slf_vtable(slf_deconstructor_play_prerender_str, slf_action_play_prerender_str, static_cast<DWORD*>((void*)0x89a8b4));
    hook_slf_vtable(slf_deconstructor_pop_pack_str, slf_action_pop_pack_str, static_cast<DWORD*>((void*)0x89c344));
    hook_slf_vtable(slf_deconstructor_post_message_str_num, slf_action_post_message_str_num, static_cast<DWORD*>((void*)0x89a73c));
    hook_slf_vtable(slf_deconstructor_pre_roll_all_pfx_num, slf_action_pre_roll_all_pfx_num, static_cast<DWORD*>((void*)0x89aa34));
    hook_slf_vtable(slf_deconstructor_press_controller_gauge_num, slf_action_press_controller_gauge_num, static_cast<DWORD*>((void*)0x89bb28));
    hook_slf_vtable(slf_deconstructor_press_controller_gauge_num_num_num, slf_action_press_controller_gauge_num_num_num, static_cast<DWORD*>((void*)0x89bb30));
    hook_slf_vtable(slf_deconstructor_purge_district_num, slf_action_purge_district_num, static_cast<DWORD*>((void*)0x89c4bc));
    hook_slf_vtable(slf_deconstructor_push_pack_str, slf_action_push_pack_str, static_cast<DWORD*>((void*)0x89c334));
    hook_slf_vtable(slf_deconstructor_push_pack_into_district_slot_str, slf_action_push_pack_into_district_slot_str, static_cast<DWORD*>((void*)0x89c33c));
    hook_slf_vtable(slf_deconstructor_raise_hotpursuit_indicator_level, slf_action_raise_hotpursuit_indicator_level, static_cast<DWORD*>((void*)0x89bad8));
    hook_slf_vtable(slf_deconstructor_random_num, slf_action_random_num, static_cast<DWORD*>((void*)0x89a744));
    hook_slf_vtable(slf_deconstructor_remove_civilian_info_num, slf_action_remove_civilian_info_num, static_cast<DWORD*>((void*)0x89c5c4));
    hook_slf_vtable(slf_deconstructor_remove_civilian_info_entity_entity_num, slf_action_remove_civilian_info_entity_entity_num, static_cast<DWORD*>((void*)0x89c5d4));
    hook_slf_vtable(slf_deconstructor_remove_glass_house_str, slf_action_remove_glass_house_str, static_cast<DWORD*>((void*)0x89a568));
    hook_slf_vtable(slf_deconstructor_remove_item_entity_from_world_entity, slf_action_remove_item_entity_from_world_entity, static_cast<DWORD*>((void*)0x89affc));
    hook_slf_vtable(slf_deconstructor_remove_pack_str, slf_action_remove_pack_str, static_cast<DWORD*>((void*)0x89c3dc));
    hook_slf_vtable(slf_deconstructor_remove_traffic_model_num, slf_action_remove_traffic_model_num, static_cast<DWORD*>((void*)0x89c5ac));
    hook_slf_vtable(slf_deconstructor_reset_externed_alses, slf_action_reset_externed_alses, static_cast<DWORD*>((void*)0x89b064));
    hook_slf_vtable(slf_deconstructor_set_all_anchors_activated_num, slf_action_set_all_anchors_activated_num, static_cast<DWORD*>((void*)0x89caec));
    hook_slf_vtable(slf_deconstructor_set_blur_num, slf_action_set_blur_num, static_cast<DWORD*>((void*)0x89a63c));
    hook_slf_vtable(slf_deconstructor_set_blur_blend_mode_num, slf_action_set_blur_blend_mode_num, static_cast<DWORD*>((void*)0x89a664));
    hook_slf_vtable(slf_deconstructor_set_blur_color_vector3d, slf_action_set_blur_color_vector3d, static_cast<DWORD*>((void*)0x89a644));
    hook_slf_vtable(slf_deconstructor_set_blur_offset_num_num, slf_action_set_blur_offset_num_num, static_cast<DWORD*>((void*)0x89a654));
    hook_slf_vtable(slf_deconstructor_set_blur_rot_num, slf_action_set_blur_rot_num, static_cast<DWORD*>((void*)0x89a65c));
    hook_slf_vtable(slf_deconstructor_set_blur_scale_num_num, slf_action_set_blur_scale_num_num, static_cast<DWORD*>((void*)0x89a64c));
    hook_slf_vtable(slf_deconstructor_set_clear_color_vector3d, slf_action_set_clear_color_vector3d, static_cast<DWORD*>((void*)0x89a6f4));
    hook_slf_vtable(slf_deconstructor_set_current_mission_objective_caption_num, slf_action_set_current_mission_objective_caption_num, static_cast<DWORD*>((void*)0x89cadc));
    hook_slf_vtable(slf_deconstructor_set_default_traffic_hitpoints_num, slf_action_set_default_traffic_hitpoints_num, static_cast<DWORD*>((void*)0x89c614));
    hook_slf_vtable(slf_deconstructor_set_dialog_box_flavor_num, slf_action_set_dialog_box_flavor_num, static_cast<DWORD*>((void*)0x89bc5c));
    hook_slf_vtable(slf_deconstructor_set_dialog_box_lockout_time_num, slf_action_set_dialog_box_lockout_time_num, static_cast<DWORD*>((void*)0x89bc6c));
    hook_slf_vtable(slf_deconstructor_set_engine_property_str_num, slf_action_set_engine_property_str_num, static_cast<DWORD*>((void*)0x89a99c));
    hook_slf_vtable(slf_deconstructor_set_fov_num, slf_action_set_fov_num, static_cast<DWORD*>((void*)0x89a62c));
    hook_slf_vtable(slf_deconstructor_set_game_info_num_str_num, slf_action_set_game_info_num_str_num, static_cast<DWORD*>((void*)0x89a8bc));
    hook_slf_vtable(slf_deconstructor_set_game_info_str_str_str, slf_action_set_game_info_str_str_str, static_cast<DWORD*>((void*)0x89a8cc));
    hook_slf_vtable(slf_deconstructor_set_global_time_dilation_num, slf_action_set_global_time_dilation_num, static_cast<DWORD*>((void*)0x89a89c));
    hook_slf_vtable(slf_deconstructor_set_marky_cam_lookat_vector3d, slf_action_set_marky_cam_lookat_vector3d, static_cast<DWORD*>((void*)0x89a5ec));
    hook_slf_vtable(slf_deconstructor_set_max_streaming_distance_num, slf_action_set_max_streaming_distance_num, static_cast<DWORD*>((void*)0x89c4b4));
    hook_slf_vtable(slf_deconstructor_set_mission_key_pos_facing_vector3d_vector3d, slf_action_set_mission_key_pos_facing_vector3d_vector3d, static_cast<DWORD*>((void*)0x89c37c));
    hook_slf_vtable(slf_deconstructor_set_mission_key_position_vector3d, slf_action_set_mission_key_position_vector3d, static_cast<DWORD*>((void*)0x89c374));
    hook_slf_vtable(slf_deconstructor_set_mission_text_num_elip, slf_action_set_mission_text_num_elip, static_cast<DWORD*>((void*)0x89bbf8));
    hook_slf_vtable(slf_deconstructor_set_mission_text_box_flavor_num, slf_action_set_mission_text_box_flavor_num, static_cast<DWORD*>((void*)0x89bc64));
    hook_slf_vtable(slf_deconstructor_set_mission_text_debug_str, slf_action_set_mission_text_debug_str, static_cast<DWORD*>((void*)0x89bc00));
    hook_slf_vtable(slf_deconstructor_set_parking_density_num, slf_action_set_parking_density_num, static_cast<DWORD*>((void*)0x89c60c));
    hook_slf_vtable(slf_deconstructor_set_pedestrian_density_num, slf_action_set_pedestrian_density_num, static_cast<DWORD*>((void*)0x89c5f4));
    hook_slf_vtable(slf_deconstructor_set_render_opt_num_str_num, slf_action_set_render_opt_num_str_num, static_cast<DWORD*>((void*)0x89a8dc));
    hook_slf_vtable(slf_deconstructor_set_score_widget_score_num, slf_action_set_score_widget_score_num, static_cast<DWORD*>((void*)0x89bac8));
    hook_slf_vtable(slf_deconstructor_set_sound_category_volume_num_num_num, slf_action_set_sound_category_volume_num_num_num, static_cast<DWORD*>((void*)0x89a9d4));
    hook_slf_vtable(slf_deconstructor_set_spider_reflexes_blur_num, slf_action_set_spider_reflexes_blur_num, static_cast<DWORD*>((void*)0x89a674));
    hook_slf_vtable(slf_deconstructor_set_spider_reflexes_blur_blend_mode_num, slf_action_set_spider_reflexes_blur_blend_mode_num, static_cast<DWORD*>((void*)0x89a69c));
    hook_slf_vtable(slf_deconstructor_set_spider_reflexes_blur_color_vector3d, slf_action_set_spider_reflexes_blur_color_vector3d, static_cast<DWORD*>((void*)0x89a67c));
    hook_slf_vtable(slf_deconstructor_set_spider_reflexes_blur_offset_num_num, slf_action_set_spider_reflexes_blur_offset_num_num, static_cast<DWORD*>((void*)0x89a68c));
    hook_slf_vtable(slf_deconstructor_set_spider_reflexes_blur_rot_num, slf_action_set_spider_reflexes_blur_rot_num, static_cast<DWORD*>((void*)0x89a694));
    hook_slf_vtable(slf_deconstructor_set_spider_reflexes_blur_scale_num_num, slf_action_set_spider_reflexes_blur_scale_num_num, static_cast<DWORD*>((void*)0x89a684));
    hook_slf_vtable(slf_deconstructor_set_spider_reflexes_hero_meter_depletion_rate_num, slf_action_set_spider_reflexes_hero_meter_depletion_rate_num, static_cast<DWORD*>((void*)0x89cab4));
    hook_slf_vtable(slf_deconstructor_set_spider_reflexes_spiderman_time_dilation_num, slf_action_set_spider_reflexes_spiderman_time_dilation_num, static_cast<DWORD*>((void*)0x89cabc));
    hook_slf_vtable(slf_deconstructor_set_spider_reflexes_world_time_dilation_num, slf_action_set_spider_reflexes_world_time_dilation_num, static_cast<DWORD*>((void*)0x89cacc));
    hook_slf_vtable(slf_deconstructor_set_state_of_the_story_caption_num, slf_action_set_state_of_the_story_caption_num, static_cast<DWORD*>((void*)0x89cae4));
    hook_slf_vtable(slf_deconstructor_set_target_info_entity_vector3d_vector3d, slf_action_set_target_info_entity_vector3d_vector3d, static_cast<DWORD*>((void*)0x89b6c4));
    hook_slf_vtable(slf_deconstructor_set_time_of_day_num, slf_action_set_time_of_day_num, static_cast<DWORD*>((void*)0x89a964));
    hook_slf_vtable(slf_deconstructor_set_traffic_density_num, slf_action_set_traffic_density_num, static_cast<DWORD*>((void*)0x89c604));
    hook_slf_vtable(slf_deconstructor_set_traffic_model_usage_num_num, slf_action_set_traffic_model_usage_num_num, static_cast<DWORD*>((void*)0x89c5b4));
    hook_slf_vtable(slf_deconstructor_set_vibration_resume_num, slf_action_set_vibration_resume_num, static_cast<DWORD*>((void*)0x89a7d4));
    hook_slf_vtable(slf_deconstructor_set_whoosh_interp_rate_num, slf_action_set_whoosh_interp_rate_num, static_cast<DWORD*>((void*)0x89b504));
    hook_slf_vtable(slf_deconstructor_set_whoosh_pitch_range_num_num, slf_action_set_whoosh_pitch_range_num_num, static_cast<DWORD*>((void*)0x89b4fc));
    hook_slf_vtable(slf_deconstructor_set_whoosh_speed_range_num_num, slf_action_set_whoosh_speed_range_num_num, static_cast<DWORD*>((void*)0x89b4ec));
    hook_slf_vtable(slf_deconstructor_set_whoosh_volume_range_num_num, slf_action_set_whoosh_volume_range_num_num, static_cast<DWORD*>((void*)0x89b4f4));
    hook_slf_vtable(slf_deconstructor_set_zoom_num, slf_action_set_zoom_num, static_cast<DWORD*>((void*)0x89a624));
    hook_slf_vtable(slf_deconstructor_show_controller_gauge, slf_action_show_controller_gauge, static_cast<DWORD*>((void*)0x89bb18));
    hook_slf_vtable(slf_deconstructor_show_hotpursuit_indicator_num, slf_action_show_hotpursuit_indicator_num, static_cast<DWORD*>((void*)0x89bad0));
    hook_slf_vtable(slf_deconstructor_show_score_widget_num, slf_action_show_score_widget_num, static_cast<DWORD*>((void*)0x89bac0));
    hook_slf_vtable(slf_deconstructor_shut_up_all_ai_voice_boxes, slf_action_shut_up_all_ai_voice_boxes, static_cast<DWORD*>((void*)0x89b50c));
    hook_slf_vtable(slf_deconstructor_sin_num, slf_action_sin_num, static_cast<DWORD*>((void*)0x89a904));
    hook_slf_vtable(slf_deconstructor_sin_cos_num, slf_action_sin_cos_num, static_cast<DWORD*>((void*)0x89a924));
    hook_slf_vtable(slf_deconstructor_soft_load_num, slf_action_soft_load_num, static_cast<DWORD*>((void*)0x89bcbc));
    hook_slf_vtable(slf_deconstructor_soft_save_num, slf_action_soft_save_num, static_cast<DWORD*>((void*)0x89bcb4));
    hook_slf_vtable(slf_deconstructor_spiderman_add_hero_points_num, slf_action_spiderman_add_hero_points_num, static_cast<DWORD*>((void*)0x89caa4));
    hook_slf_vtable(slf_deconstructor_spiderman_bank_stylepoints, slf_action_spiderman_bank_stylepoints, static_cast<DWORD*>((void*)0x89c91c));
    hook_slf_vtable(slf_deconstructor_spiderman_break_web, slf_action_spiderman_break_web, static_cast<DWORD*>((void*)0x89c9e4));
    hook_slf_vtable(slf_deconstructor_spiderman_camera_add_shake_num_num_num, slf_action_spiderman_camera_add_shake_num_num_num, static_cast<DWORD*>((void*)0x89cb34));
    hook_slf_vtable(slf_deconstructor_spiderman_camera_autocorrect_num, slf_action_spiderman_camera_autocorrect_num, static_cast<DWORD*>((void*)0x89c924));
    hook_slf_vtable(slf_deconstructor_spiderman_camera_clear_fixedstatic, slf_action_spiderman_camera_clear_fixedstatic, static_cast<DWORD*>((void*)0x89cafc));
    hook_slf_vtable(slf_deconstructor_spiderman_camera_enable_combat_num, slf_action_spiderman_camera_enable_combat_num, static_cast<DWORD*>((void*)0x89cb24));
    hook_slf_vtable(slf_deconstructor_spiderman_camera_enable_lookaround_num, slf_action_spiderman_camera_enable_lookaround_num, static_cast<DWORD*>((void*)0x89cb1c));
    hook_slf_vtable(slf_deconstructor_spiderman_camera_set_fixedstatic_vector3d_vector3d, slf_action_spiderman_camera_set_fixedstatic_vector3d_vector3d, static_cast<DWORD*>((void*)0x89caf4));
    hook_slf_vtable(slf_deconstructor_spiderman_camera_set_follow_entity, slf_action_spiderman_camera_set_follow_entity, static_cast<DWORD*>((void*)0x89cb2c));
    hook_slf_vtable(slf_deconstructor_spiderman_camera_set_hero_underwater_num, slf_action_spiderman_camera_set_hero_underwater_num, static_cast<DWORD*>((void*)0x89cb3c));
    hook_slf_vtable(slf_deconstructor_spiderman_camera_set_interpolation_time_num, slf_action_spiderman_camera_set_interpolation_time_num, static_cast<DWORD*>((void*)0x89cb14));
    hook_slf_vtable(slf_deconstructor_spiderman_camera_set_lockon_min_distance_num, slf_action_spiderman_camera_set_lockon_min_distance_num, static_cast<DWORD*>((void*)0x89cb04));
    hook_slf_vtable(slf_deconstructor_spiderman_camera_set_lockon_y_offset_num, slf_action_spiderman_camera_set_lockon_y_offset_num, static_cast<DWORD*>((void*)0x89cb0c));
    hook_slf_vtable(slf_deconstructor_spiderman_charged_jump, slf_action_spiderman_charged_jump, static_cast<DWORD*>((void*)0x89c98c));
    hook_slf_vtable(slf_deconstructor_spiderman_enable_control_button_num_num, slf_action_spiderman_enable_control_button_num_num, static_cast<DWORD*>((void*)0x89ca94));
    hook_slf_vtable(slf_deconstructor_spiderman_enable_lockon_num, slf_action_spiderman_enable_lockon_num, static_cast<DWORD*>((void*)0x89c9ac));
    hook_slf_vtable(slf_deconstructor_spiderman_engage_lockon_num, slf_action_spiderman_engage_lockon_num, static_cast<DWORD*>((void*)0x89c9b4));
    hook_slf_vtable(slf_deconstructor_spiderman_engage_lockon_num_entity, slf_action_spiderman_engage_lockon_num_entity, static_cast<DWORD*>((void*)0x89c9bc));
    hook_slf_vtable(slf_deconstructor_spiderman_get_hero_points, slf_action_spiderman_get_hero_points, static_cast<DWORD*>((void*)0x89ca9c));
    hook_slf_vtable(slf_deconstructor_spiderman_get_max_zip_length, slf_action_spiderman_get_max_zip_length, static_cast<DWORD*>((void*)0x89c9dc));
    hook_slf_vtable(slf_deconstructor_spiderman_get_spidey_sense_level, slf_action_spiderman_get_spidey_sense_level, static_cast<DWORD*>((void*)0x89c99c));
    hook_slf_vtable(slf_deconstructor_spiderman_is_crawling, slf_action_spiderman_is_crawling, static_cast<DWORD*>((void*)0x89c934));
    hook_slf_vtable(slf_deconstructor_spiderman_is_falling, slf_action_spiderman_is_falling, static_cast<DWORD*>((void*)0x89c964));
    hook_slf_vtable(slf_deconstructor_spiderman_is_jumping, slf_action_spiderman_is_jumping, static_cast<DWORD*>((void*)0x89c96c));
    hook_slf_vtable(slf_deconstructor_spiderman_is_on_ceiling, slf_action_spiderman_is_on_ceiling, static_cast<DWORD*>((void*)0x89c944));
    hook_slf_vtable(slf_deconstructor_spiderman_is_on_ground, slf_action_spiderman_is_on_ground, static_cast<DWORD*>((void*)0x89c94c));
    hook_slf_vtable(slf_deconstructor_spiderman_is_on_wall, slf_action_spiderman_is_on_wall, static_cast<DWORD*>((void*)0x89c93c));
    hook_slf_vtable(slf_deconstructor_spiderman_is_running, slf_action_spiderman_is_running, static_cast<DWORD*>((void*)0x89c95c));
    hook_slf_vtable(slf_deconstructor_spiderman_is_sprint_crawling, slf_action_spiderman_is_sprint_crawling, static_cast<DWORD*>((void*)0x89c984));
    hook_slf_vtable(slf_deconstructor_spiderman_is_sprinting, slf_action_spiderman_is_sprinting, static_cast<DWORD*>((void*)0x89c974));
    hook_slf_vtable(slf_deconstructor_spiderman_is_swinging, slf_action_spiderman_is_swinging, static_cast<DWORD*>((void*)0x89c954));
    hook_slf_vtable(slf_deconstructor_spiderman_is_wallsprinting, slf_action_spiderman_is_wallsprinting, static_cast<DWORD*>((void*)0x89c97c));
    hook_slf_vtable(slf_deconstructor_spiderman_lock_spider_reflexes_off, slf_action_spiderman_lock_spider_reflexes_off, static_cast<DWORD*>((void*)0x89ca24));
    hook_slf_vtable(slf_deconstructor_spiderman_lock_spider_reflexes_on, slf_action_spiderman_lock_spider_reflexes_on, static_cast<DWORD*>((void*)0x89ca1c));
    hook_slf_vtable(slf_deconstructor_spiderman_lockon_camera_engaged, slf_action_spiderman_lockon_camera_engaged, static_cast<DWORD*>((void*)0x89ca0c));
    hook_slf_vtable(slf_deconstructor_spiderman_lockon_mode_engaged, slf_action_spiderman_lockon_mode_engaged, static_cast<DWORD*>((void*)0x89ca04));
    hook_slf_vtable(slf_deconstructor_spiderman_set_camera_target_entity, slf_action_spiderman_set_camera_target_entity, static_cast<DWORD*>((void*)0x89ca14));
    hook_slf_vtable(slf_deconstructor_spiderman_set_desired_mode_num_vector3d_vector3d, slf_action_spiderman_set_desired_mode_num_vector3d_vector3d, static_cast<DWORD*>((void*)0x89c9ec));
    hook_slf_vtable(slf_deconstructor_spiderman_set_health_beep_min_max_cooldown_time_num_num, slf_action_spiderman_set_health_beep_min_max_cooldown_time_num_num, static_cast<DWORD*>((void*)0x89c9f4));
    hook_slf_vtable(slf_deconstructor_spiderman_set_health_beep_threshold_num, slf_action_spiderman_set_health_beep_threshold_num, static_cast<DWORD*>((void*)0x89c9fc));
    hook_slf_vtable(slf_deconstructor_spiderman_set_hero_meter_empty_rate_num, slf_action_spiderman_set_hero_meter_empty_rate_num, static_cast<DWORD*>((void*)0x89cb44));
    hook_slf_vtable(slf_deconstructor_spiderman_set_max_height_num, slf_action_spiderman_set_max_height_num, static_cast<DWORD*>((void*)0x89c9cc));
    hook_slf_vtable(slf_deconstructor_spiderman_set_max_zip_length_num, slf_action_spiderman_set_max_zip_length_num, static_cast<DWORD*>((void*)0x89c9d4));
    hook_slf_vtable(slf_deconstructor_spiderman_set_min_height_num, slf_action_spiderman_set_min_height_num, static_cast<DWORD*>((void*)0x89c9c4));
    hook_slf_vtable(slf_deconstructor_spiderman_set_spidey_sense_level_num, slf_action_spiderman_set_spidey_sense_level_num, static_cast<DWORD*>((void*)0x89c994));
    hook_slf_vtable(slf_deconstructor_spiderman_set_swing_anchor_max_sticky_time_num, slf_action_spiderman_set_swing_anchor_max_sticky_time_num, static_cast<DWORD*>((void*)0x89c9a4));
    hook_slf_vtable(slf_deconstructor_spiderman_subtract_hero_points_num, slf_action_spiderman_subtract_hero_points_num, static_cast<DWORD*>((void*)0x89caac));
    hook_slf_vtable(slf_deconstructor_spiderman_td_set_alternating_wall_run_occurrence_threshold_num, slf_action_spiderman_td_set_alternating_wall_run_occurrence_threshold_num, static_cast<DWORD*>((void*)0x89ca74));
    hook_slf_vtable(slf_deconstructor_spiderman_td_set_alternating_wall_run_time_threshold_num, slf_action_spiderman_td_set_alternating_wall_run_time_threshold_num, static_cast<DWORD*>((void*)0x89ca6c));
    hook_slf_vtable(slf_deconstructor_spiderman_td_set_big_air_height_threshold_num, slf_action_spiderman_td_set_big_air_height_threshold_num, static_cast<DWORD*>((void*)0x89ca34));
    hook_slf_vtable(slf_deconstructor_spiderman_td_set_continuous_air_swings_threshold_num, slf_action_spiderman_td_set_continuous_air_swings_threshold_num, static_cast<DWORD*>((void*)0x89ca4c));
    hook_slf_vtable(slf_deconstructor_spiderman_td_set_gain_altitude_height_threshold_num, slf_action_spiderman_td_set_gain_altitude_height_threshold_num, static_cast<DWORD*>((void*)0x89ca54));
    hook_slf_vtable(slf_deconstructor_spiderman_td_set_near_miss_trigger_radius_num, slf_action_spiderman_td_set_near_miss_trigger_radius_num, static_cast<DWORD*>((void*)0x89ca84));
    hook_slf_vtable(slf_deconstructor_spiderman_td_set_near_miss_velocity_threshold_num, slf_action_spiderman_td_set_near_miss_velocity_threshold_num, static_cast<DWORD*>((void*)0x89ca8c));
    hook_slf_vtable(slf_deconstructor_spiderman_td_set_orbit_min_radius_threshold_num, slf_action_spiderman_td_set_orbit_min_radius_threshold_num, static_cast<DWORD*>((void*)0x89ca3c));
    hook_slf_vtable(slf_deconstructor_spiderman_td_set_soft_landing_velocity_threshold_num, slf_action_spiderman_td_set_soft_landing_velocity_threshold_num, static_cast<DWORD*>((void*)0x89ca5c));
    hook_slf_vtable(slf_deconstructor_spiderman_td_set_super_speed_speed_threshold_num, slf_action_spiderman_td_set_super_speed_speed_threshold_num, static_cast<DWORD*>((void*)0x89ca7c));
    hook_slf_vtable(slf_deconstructor_spiderman_td_set_swinging_wall_run_time_threshold_num, slf_action_spiderman_td_set_swinging_wall_run_time_threshold_num, static_cast<DWORD*>((void*)0x89ca64));
    hook_slf_vtable(slf_deconstructor_spiderman_td_set_wall_sprint_time_threshold_num, slf_action_spiderman_td_set_wall_sprint_time_threshold_num, static_cast<DWORD*>((void*)0x89ca44));
    hook_slf_vtable(slf_deconstructor_spiderman_unlock_spider_reflexes, slf_action_spiderman_unlock_spider_reflexes, static_cast<DWORD*>((void*)0x89ca2c));
    hook_slf_vtable(slf_deconstructor_spiderman_wait_add_threat_entity_str_num_num, slf_action_spiderman_wait_add_threat_entity_str_num_num, static_cast<DWORD*>((void*)0x89cb4c));
    hook_slf_vtable(slf_deconstructor_spidey_can_see_vector3d, slf_action_spidey_can_see_vector3d, static_cast<DWORD*>((void*)0x89c92c));
    hook_slf_vtable(slf_deconstructor_sqrt_num, slf_action_sqrt_num, static_cast<DWORD*>((void*)0x89a914));
    hook_slf_vtable(slf_deconstructor_start_patrol_str, slf_action_start_patrol_str, static_cast<DWORD*>((void*)0x89c4f4));
    hook_slf_vtable(slf_deconstructor_stop_all_sounds, slf_action_stop_all_sounds, static_cast<DWORD*>((void*)0x89b514));
    hook_slf_vtable(slf_deconstructor_stop_credits, slf_action_stop_credits, static_cast<DWORD*>((void*)0x89bb00));
    hook_slf_vtable(slf_deconstructor_stop_vibration, slf_action_stop_vibration, static_cast<DWORD*>((void*)0x89a7cc));
    hook_slf_vtable(slf_deconstructor_subtitle_num_num_num_num_num_num, slf_action_subtitle_num_num_num_num_num_num, static_cast<DWORD*>((void*)0x89bcc4));
    hook_slf_vtable(slf_deconstructor_swap_hero_costume_str, slf_action_swap_hero_costume_str, static_cast<DWORD*>((void*)0x89c4d4));
    hook_slf_vtable(slf_deconstructor_text_width_str, slf_action_text_width_str, static_cast<DWORD*>((void*)0x89a7ac));
    hook_slf_vtable(slf_deconstructor_timer_widget_get_count_up, slf_action_timer_widget_get_count_up, static_cast<DWORD*>((void*)0x89bb70));
    hook_slf_vtable(slf_deconstructor_timer_widget_get_time, slf_action_timer_widget_get_time, static_cast<DWORD*>((void*)0x89bb60));
    hook_slf_vtable(slf_deconstructor_timer_widget_set_count_up_num, slf_action_timer_widget_set_count_up_num, static_cast<DWORD*>((void*)0x89bb68));
    hook_slf_vtable(slf_deconstructor_timer_widget_set_time_num, slf_action_timer_widget_set_time_num, static_cast<DWORD*>((void*)0x89bb58));
    hook_slf_vtable(slf_deconstructor_timer_widget_start, slf_action_timer_widget_start, static_cast<DWORD*>((void*)0x89bb48));
    hook_slf_vtable(slf_deconstructor_timer_widget_stop, slf_action_timer_widget_stop, static_cast<DWORD*>((void*)0x89bb50));
    hook_slf_vtable(slf_deconstructor_timer_widget_turn_off, slf_action_timer_widget_turn_off, static_cast<DWORD*>((void*)0x89bb40));
    hook_slf_vtable(slf_deconstructor_timer_widget_turn_on, slf_action_timer_widget_turn_on, static_cast<DWORD*>((void*)0x89bb38));




   

}



void close_debug()
{
        debug_enabled = 0;
        menu_disabled = 0;
        game_unpause(g_game_ptr());
        g_game_ptr()->enable_physics(true);
}




void handle_debug_entry(debug_menu_entry* entry, custom_key_type) {
	current_menu = static_cast<decltype(current_menu)>(entry->data);
}

typedef bool (__fastcall *entity_tracker_manager_get_the_arrow_target_pos_ptr)(void *, void *, vector3d *);
entity_tracker_manager_get_the_arrow_target_pos_ptr entity_tracker_manager_get_the_arrow_target_pos = (entity_tracker_manager_get_the_arrow_target_pos_ptr) 0x0062EE10;

void set_god_mode(int a1)
{
    CDECL_CALL(0x004BC040, a1);
}



typedef void* (__fastcall* script_instance_add_thread_ptr)(script_instance* , void* edx, vm_executable* a1, void* a2);
script_instance_add_thread_ptr script_instance_add_thread = (script_instance_add_thread_ptr) 0x005AAD00;

void handle_progression_select_entry(debug_menu_entry* entry) {

	script_instance* instance = static_cast<script_instance *>(entry->data);
	int functionid = (int) entry->data1;

	DWORD addr = (DWORD) entry;

	script_instance_add_thread(instance, nullptr, instance->object->funcs[functionid], &addr);

	close_debug();
}





void debug_menu_enabled()
{
        debug_enabled = 1;
        menu_disabled = 1;
        game_unpause(g_game_ptr());
        g_game_ptr()->enable_physics(false);
}

void devopt_flags_handler(debug_menu_entry* a1)
{
        switch (a1->get_id()) {
        case 0u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "CD_ONLY" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "CD_ONLY" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 1u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "ENVMAP_TOOL" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "ENVMAP_TOOL" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 2u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "CHATTY_LOAD" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "CHATTY_LOAD" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 3u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "NO_CD" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "NO_CD" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 4u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "WINDOW_DEFAULT" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "WINDOW_DEFAULT" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 5u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_FPS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_FPS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 6u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_STREAMER_INFO" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_STREAMER_INFO" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 7u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_STREAMER_SPAM" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_STREAMER_SPAM" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 8u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_RESOURCE_SPAM" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_RESOURCE_SPAM" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 9u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_MEMORY_INFO" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_MEMORY_INFO" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 10u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_SPIDEY_SPEED" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_SPIDEY_SPEED" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 11u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "TRACE_MISSION_MANAGER" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "TRACE_MISSION_MANAGER" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 12u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "DUMP_MISSION_HEAP" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "DUMP_MISSION_HEAP" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 13u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "CAMERA_CENTRIC_STREAMER" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "CAMERA_CENTRIC_STREAMER" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 14u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "RENDER_LOWLODS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "RENDER_LOWLODS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 15u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "LOAD_STRING_HASH_DICTIONARY" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "LOAD_STRING_HASH_DICTIONARY" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 16u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "LOG_RUNTIME_STRING_HASHES" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "LOG_RUNTIME_STRING_HASHES" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 17u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_WATERMARK_VELOCITY" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_WATERMARK_VELOCITY" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 18u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_STATS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_STATS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 19u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "ENABLE_ZOOM_MAP" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "ENABLE_ZOOM_MAP" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 20u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_DEBUG_INFO" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_DEBUG_INFO" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 21u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_PROFILE_INFO" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_PROFILE_INFO" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 22u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_LOCOMOTION_INFO" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_LOCOMOTION_INFO" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 23u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "GRAVITY" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "GRAVITY" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 24u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "TEST_MEMORY_LEAKS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "TEST_MEMORY_LEAKS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 25u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "HALT_ON_ASSERTS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "HALT_ON_ASSERTS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 26u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SCREEN_ASSERTS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SCREEN_ASSERTS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 27u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "NO_ANIM_WARNINGS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "NO_ANIM_WARNINGS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 28u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "PROFILING_ON" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "PROFILING_ON" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 29u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "MONO_AUDIO" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "MONO_AUDIO" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 30u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "NO_MESSAGES" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "NO_MESSAGES" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 31u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "LOCK_STEP" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "LOCK_STEP" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 32u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "TEXTURE_DUMP" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "TEXTURE_DUMP" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 33u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "DISABLE_SOUND_WARNINGS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "DISABLE_SOUND_WARNINGS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 34u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "DISABLE_SOUND_DEBUG_OUTPUT" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "DISABLE_SOUND_DEBUG_OUTPUT" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 35u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "DELETE_UNUSED_SOUND_BANKS_ON_PACK" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "DELETE_UNUSED_SOUND_BANKS_ON_PACK" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 36u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "LOCKED_HERO" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "LOCKED_HERO" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 37u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "FOG_OVERR_IDE" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "FOG_OVERR IDE" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 38u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "FOG_DISABLE" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "FOG_DISABLE" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 39u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "MOVE_EDITOR" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "MOVE_EDITOR" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 40u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "AI_PATH_DEBUG" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "AI_PATH_DEBUG" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 41u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "AI_PATH_COLOR" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "AI_PATH_COLOR" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 42u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "AI_CRITTER_ACTIVITY" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "AI_CRITTER_ACTIVITY" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 43u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "AI_PATH_COLOR_CRITTER" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "AI_PATH_COLOR_CRITTER" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 44u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "AI_PATH_COLOR_HERO" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "AI_PATH_COLOR_HERO" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 45u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "NO_PARTICLES" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "NO_PARTICLES" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 46u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "NO_PARTICLE_PUMP" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "NO_PARTICLE_PUMP" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 47u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_SHADOW_BALL" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_SHADOW_BALL" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 48u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_LIGHTS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_LIGHTS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 49u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_PLRCTRL" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_PLRCTRL" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 50u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_PSX_INFO" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_PSX_INFO" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 51u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "FLAT_SHADE" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "FLAT_SHADE" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 52u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "INTERFACE_DISABLE" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "INTERFACE_DISABLE" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 53u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "WIDGET_TOOLS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "WIDGET_TOOLS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 54u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "LIGHTING" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "LIGHTING" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 55u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "FAKE_POINT_LIGHTS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "FAKE_POINT_LIGHTS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 56u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "BSP_SPRAY_PAINT" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "BSP_SPRAY_PAINT" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 57u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "CAMERA_EDITOR" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "CAMERA_EDITOR" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 58u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "IGNORE_RAMPING" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "IGNORE_RAMPING" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 59u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "POINT_SAMPLE" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "POINT_SAMPLE" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 60u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "DISTANCE_FADING" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "DISTANCE_FADING" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 61u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "OVERRIDE_CONTROLLER_OPTIONS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "OVERRIDE_CONTROLLER_OPTIONS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 62u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "DISABLE_MOUSE_PLAYER_CONTROL" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "DISABLE_MOUSE_PLAYER_CONTROL" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 63u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "NO_MOVIES" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "NO_MOVIES" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 64u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "XBOX_USER_CAM" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "XBOX_USER_CAM" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 65u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "NO_LOAD_SCREEN" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "NO_LOAD_SCREEN" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 66u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "EXCEPTION_HANDLER" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "EXCEPTION_HANDLER" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 67u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "NO_EXCEPTION_HANDLER" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "NO_EXCEPTION_HANDLER" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 68u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "NO_CD_CHECK" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "NO_CD_CHECK" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 69u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "NO_LOAD_METER" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "NO_LOAD_METER" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 70u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "NO_MOVIE_BUFFER_WARNING" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "NO_MOVIE_BUFFER_WARNING" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 71u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "LIMITED_EDITION_DISC" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "LIMITED_EDITION_DISC" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 72u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "NEW_COMBAT_LOCO" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "NEW_COMBAT_LOCO" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 73u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "LEVEL_WARP" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "LEVEL_WARP" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 74u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SMOKE_TEST" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SMOKE_TEST" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 75u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SMOKE_TEST_LEVEL" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SMOKE_TEST_LEVEL" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 76u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "COMBO_TESTER" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "COMBO_TESTER" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 77u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "DROP_SHADOWS_ALWAYS_RAYCAST" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "DROP_SHADOWS_ALWAYS_RAYCAST" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 78u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "DISABLE_DROP_SHADOWS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "DISABLE_DROP_SHADOWS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 79u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "DISABLE_HIRES_SHADOWS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "DISABLE_HIRES_SHADOWS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 80u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "DISABLE_STENCIL_SHADOWS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "DISABLE_STENCIL_SHADOWS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 81u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "DISABLE_COLORVOLS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "DISABLE_COLORVOLS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 82u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "DISABLE_RENDER_ENTS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "DISABLE_RENDER_ENTS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 83u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "DISABLE_FULLSCREEN_BLUR" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "DISABLE_FULLSCREEN_BLUR" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 84u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "FORCE_TIGHTCRAWL" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "FORCE_TIGHTCRAWL" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 85u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "FORCE_NONCRAWL" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "FORCE_NONCRAWL" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 86u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_DEBUG_TEXT" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_DEBUG_TEXT" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 87u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_STYLE_POINTS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_STYLE_POINTS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 88u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "CAMERA_MOUSE_MODE" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "CAMERA_MOUSE_MODE" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 89u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "USERCAM_ON_CONTROLLER2" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "USERCAM_ON_CONTROLLER2" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 90u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "DISABLE_ANCHOR_RETICLE_RENDERING" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "DISABLE_ANCHOR_RETICLE_RENDERING" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 91u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_ANCHOR_LINE" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_ANCHOR_LINE" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 92u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "FREE_SPIDER_REFLEXES" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "FREE_SPIDER_REFLEXES" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 93u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_BAR_OF_SHAME" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_BAR_OF_SHAME" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        case 94u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_ENEMY_HEALTH_WIDGETS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_ENEMY_HEALTH_WIDGETS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 95u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "ALLOW_IGC_PAUSE" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "ALLOW_IGC_PAUSE" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 96u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_OBBS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_OBBS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 97u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_DISTRICTS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_DISTRICTS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 98u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_CHUCK_DEBUGGER" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_CHUCK_DEBUGGER" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 99u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "CHUCK_OLD_FASHIONED" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "CHUCK_OLD_FASHIONED" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 100u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "CHUCK_DISABLE_BREAKPOINTS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "CHUCK_DISABLE_BREAKPOINTS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 101u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_AUDIO_BOXES" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_AUDIO_BOXES" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 102u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "DISABLE_SOUNDS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "DISABLE_SOUNDS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 103u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_TERRAIN_INFO" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_TERRAIN_INFO" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 104u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "DISABLE_AUDIO_BOXES" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "DISABLE_AUDIO_BOXES" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 105u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "NSL_OLD_FASHIONED" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "NSL_OLD_FASHIONED" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 106u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_MASTER_CLOCK" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_MASTER_CLOCK" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 107u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "LOAD_GRADIENTS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "LOAD_GRADIENTS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 108u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "BONUS_LEVELS_AVAILABLE" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "BONUS_LEVELS_AVAILABLE" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 109u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "COMBAT_DISPLAY" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "COMBAT_DISPLAY" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 110u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "COMBAT_DEBUGGER" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "COMBAT_DEBUGGER" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 111u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "ALLOW_ERROR_POPUPS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "ALLOW_ERROR_POPUPS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 112u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "ALLOW_WARNING_POPUPS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "ALLOW_WARNING_POPUPS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 113u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "OUTPUT_WARNING_DISABLE" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "OUTPUT_WARNING_DISABLE" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 114u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "OUTPUT_ASSERT_DISABLE" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "OUTPUT_ASSERT_DISABLE" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 115u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "ASSERT_ON_WARNING" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "ASSERT_ON_WARNING" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 116u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "ALWAYS_ACTIVE" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "ALWAYS_ACTIVE" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 117u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "FORCE_PROGRESSION" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "FORCE_PROGRESSION" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 118u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "LINK" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "LINK" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 119u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "WAIT_FOR_LINK" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "WAIT_FOR_LINK" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 120u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_END_OF_PACK" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_END_OF_PACK" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 121u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "LIVE_IN_GLASS_HOUSE" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "LIVE_IN_GLASS_HOUSE" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 122u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_GLASS_HOUSE" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_GLASS_HOUSE" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 123u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "DISABLE_RACE_PREVIEW" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "DISABLE_RACE_PREVIEW" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 124u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "FREE_MINI_MAP" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "FREE_MINI_MAP" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 125u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_LOOPING_ANIM_WARNINGS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_LOOPING_ANIM_WARNINGS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 126u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_PERF_INFO" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_PERF_INFO" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 127u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "COPY_ERROR_TO_CLIPBOARD" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "COPY_ERROR_TO_CLIPBOARD" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 128u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "BOTH_HANDS_UP_REORIENT" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "BOTH_HANDS_UP_REORIENT" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 129u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "SHOW_CAMERA_PROJECTION" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "SHOW_CAMERA_PROJECTION" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 130u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "OLD_DEFAULT_CONTROL_SETTINGS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "OLD_DEFAULT_CONTROL_SETTINGS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 131u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "IGC_SPEED_CONTROL" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "IGC_SPEED_CONTROL" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 132u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "RTDT_ENABLED" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "RTDT_ENABLED" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 133u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "ENABLE_DECALS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "ENABLE_DECALS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 134u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "AUTO_STICK_TO_WALLS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "AUTO_STICK_TO_WALLS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 135u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "ENABLE_PEDESTRIANS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "ENABLE_PEDESTRIANS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 136u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "CAMERA_INVERT_LOOKAROUND" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "CAMERA_INVERT_LOOKAROUND" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 137u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "CAMERA_INVERT_LOOKAROUND_X" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "CAMERA_INVERT_LOOKAROUND_X" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 138u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "CAMERA_INVERT_LOOKAROUND_Y" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "CAMERA_INVERT_LOOKAROUND_Y" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 139u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "FORCE_COMBAT_CAMERA_OFF" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "FORCE_COMBAT_CAMERA_OFF" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 140u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "DISPLAY_THOUGHT_BUBBLES" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "DISPLAY_THOUGHT_BUBBLES" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 141u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "ENABLE_DEBUG_LOG" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "ENABLE_DEBUG_LOG" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 142u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "ENABLE_LONG_CALLSTACK" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "ENABLE_LONG_CALLSTACK" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 143u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "RENDER_FE_UI" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "RENDER_FE_UI" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 144u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "LOCK_INTERIORS" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "LOCK_INTERIORS" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 145u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "MEMCHECK_ON_LOAD" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "MEMCHECK_ON_LOAD" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 146u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "DISPLAY_ALS_USAGE_PROFILE" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "DISPLAY_ALS_USAGE_PROFILE" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 147u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "ENABLE_FPU_EXCEPTION_HANDLING" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "ENABLE_FPU_EXCEPTION_HANDLING" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }

        case 148u: // Show Districts
        {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "UNLOCK_ALL_UNLOCKABLES" }, a1->get_bval());

        if (a1->get_bval()) {
            os_developer_options::instance()->set_flag(mString { "UNLOCK_ALL_UNLOCKABLES" }, true);
        }
        debug_menu_enabled();
        // TODO
        // sub_66C242(&g_game_ptr->field_4C);
        break;
        }
        default:
        return;
        }
}

void sub_B818C0(const float* a2, debug_menu_entry* entry)
{
        auto* v2 = entry->field_20;
        v2[0] = a2[0];
        v2[1] = a2[1];
        v2[2] = a2[2];
        v2[3] = a2[3];
}


int g_TOD_ptr = 0091E000;
void time_of_day_name_generator(debug_menu_entry* entry)
{

        DWORD lastTOD = (DWORD)entry->data;
        DWORD currentTOD = *g_TOD;
        if (currentTOD == lastTOD) {
        return;
        }

        snprintf(entry->text, MAX_CHARS, "Time of Day: %d", currentTOD);
        lastTOD = currentTOD;
}











void time_of_day_handler(debug_menu_entry* entry, custom_key_type key, menu_handler_function original)
{

        DWORD currentTOD = *g_TOD;
        switch (key) {
        case LEFT:
        us_lighting_switch_time_of_day(modulo(currentTOD - 1, 4));
        break;
        case RIGHT:
        us_lighting_switch_time_of_day(modulo(currentTOD + 1, 4));
        break;
        }
}




extern debug_menu* devopt_flags_menu = nullptr;

void handle_devopt_entry(debug_menu_entry* entry, custom_key_type key_type)
{
        printf("handle_game_entry = %s, %s, entry_type = %s\n", entry->text, to_string(key_type), to_string(entry->entry_type));

        if (key_type == ENTER) {
        switch (entry->entry_type) {
        case UNDEFINED: {
            if (entry->m_game_flags_handler != nullptr) {
                                entry->m_game_flags_handler(entry);
            }
            break;
        }
        case BOOLEAN_E:
        case POINTER_BOOL:
        case POINTER_HERO: {
            auto v3 = entry->get_bval();
            entry->set_bval(!v3, true);
            break;
        }
        case POINTER_MENU: {
            if (entry->data != nullptr) {
                                current_menu = static_cast<decltype(current_menu)>(entry->data);
            }
            return;
        }
        default:
            break;
        }
        } else if (key_type == LEFT) {
        entry->on_change(-1.0, false);
        auto v3 = entry->get_bval();
        entry->set_bval(!v3, true);
        } else if (key_type == RIGHT) {
        entry->on_change(1.0, true);
        auto v3 = entry->get_bval();
        entry->set_bval(!v3, true);
        }
}

void create_devopt_flags_menu(debug_menu* parent)
{
        if (parent->used_slots != 0) {
        return;
        }

        assert(parent != nullptr);

        devopt_flags_menu = create_menu("Devopt Flags", handle_devopt_entry, 300);
        auto* v10 = parent;

        debug_menu_entry v90 { devopt_flags_menu };
        v10->add_entry(&v90);
        static bool byte_1597BC0 = false;
        v90.set_pt_bval(&byte_1597BC0);

        // v92->add_entry(&v90);

        v90 = debug_menu_entry(mString { "CD_ONLY" });
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(0);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry(mString { "ENVMAP_TOOL" });
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(1);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "NO_CD" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(2);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "CHATTY_LOAD" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(3);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "WINDOW_DEFAULT" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(4);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SHOW_FPS" } };
        static bool show_fps = false;
        v90.set_pt_bval(&show_fps);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(5);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SHOW_STREAMER_INFO" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(6);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SHOW_STREAMER_SPAM" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(7);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SHOW_RESOURCE_SPAM" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(8);
        add_debug_menu_entry(devopt_flags_menu, &v90);


        debug_menu_entry v88 = { "SHOW MEMORY INFO", POINTER_BOOL, (void*)0x975849 };
        add_debug_menu_entry(devopt_flags_menu, &v88);

        v90 = debug_menu_entry { mString { "SHOW_SPIDEY_SPEED" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(10);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "TRACE_MISSION_MANAGER" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(11);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "DUMP_MISSION_HEAP" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(12);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "CAMERA_CENTRIC_STREAMER" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(13);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "RENDER_LOWLODS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(14);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "LOAD_STRING_HASH_DICTIONARY" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(15);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "LOG_RUNTIME_STRING_HASHES" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(16);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SHOW_WATERMARK_VELOCITY" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(17);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SHOW_STATS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(18);
        add_debug_menu_entry(devopt_flags_menu, &v90);
        v90 = debug_menu_entry { mString { "ENABLE_ZOOM_MAP" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(19);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SHOW_DEBUG_INFO" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(20);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SHOW_PROFILE_INFO" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(21);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SHOW_LOCOMOTION_INFO" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(22);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        BYTE* v91 = *(BYTE**)0x0096858C;
        debug_menu_entry v92 = { "GRAVITY", POINTER_BOOL, &v91[4 + 0x14] };
        add_debug_menu_entry(devopt_flags_menu, &v92);

        v90 = debug_menu_entry { mString { "TEST_MEMORY_LEAKS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(24);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "HALT_ON_ASSERT" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(25);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SCREEN_ASSERT" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(26);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "NO_ANIM_WARNINGS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(27);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "PROFILING_ON" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(28);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "MONO_AUDIO" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(29);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "NO_MESSAGGES" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(30);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "LOCK_STEP" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(31);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "TEXTURE_DUMP" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(32);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "DISABLE_SOUND_WARNINGS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(33);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "DISABLE_SOUND_DEBUG_OUTPUT" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(34);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "DELETE_UNUSED_SOUND_BANKS_ON_PACK" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(35);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "LOCKED_HERO" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(36);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "FOG_OVERR_IDE" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(37);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "FOG_DISABLE" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(38);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "MOVE_EDITOR" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(39);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "PATH_DEBUG" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(40);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "PATH_COLOR" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(41);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "CRITTER_ACTIVITY" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(42);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "PATH_COLOR_CRITTER" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(43);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "PATH_COLOR_HERO" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(44);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "NO_PARTICLES" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(45);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "NO_PARTICLES_PUMP" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(46);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SHOW_NORMALS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(47);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SHOW_SHADOW_BALL" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(48);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SHOW_LIGHTS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(49);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SHOW_PLRCTRL" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(50);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SHOW_PSX_INFO" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(51);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "FLAT_SHADE" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(52);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "INTERFACE_DISABLE" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(53);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "WIDGET_TOOLS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(54);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "LIGHTING" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(55);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "FAKE_POINT_LIGHTS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(56);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "BSP_SPRAY_PAINT" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(57);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "CAMERA_EDITOR" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(58);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "IGNORE_RAMPING" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(59);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "POINT_SAMPLE" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(60);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "DISTANCE_FADING" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(61);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "OVERRIDE_CONTROLLER_OPTIONS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(62);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "DISABLE_MOUSE_PLAYER_CONTROL" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(63);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "XBOX_USER_CAM" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(64);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "NO_LOAD_SCREEN" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(65);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "EXCEPTION_HANDLER" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(66);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "NO_EXCEPTION_HANDLER" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(67);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "NO_CD_CHECK" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(68);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "NO_LOAD_METER" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(69);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "NO_MOVIE_BUFFER_WARNING" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(70);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "LIMITED_EDITION_DISC" } };
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_bval(false);
        v90.set_id(75);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "NEW_COMBAT_LOCO" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(71);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "LEVEL_WARP" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(72);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SMOKE_TEST" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(73);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SMOKE_TEST_LEVEL" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(74);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "COMBO_TESTER" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(76);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "DROP_SHADOWS_ALWAYS_RAYCAST" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(77);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "DISABLE_DROP_SHADOWS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(78);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "DISABLE_HIRES_SHADOWS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(79);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "DISABLE_STENCIL_SHADOWS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(80);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "DISABLE_COLORVOLS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(81);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "DISABLE_RENDER_ENTS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(82);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "DISABLE_FULLSCREEN_BLUR" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(83);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "FORCE_TIGHTCRAWL" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(84);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "FORCE_NONCRAWL" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(85);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SHOW_DEBUG_TEXT" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(86);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SHOW_STYLE_POINTS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(87);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "CAMERA_MOUSE_MODE" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(88);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "USERCAM_ON_CONTROLLER2" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(89);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "DISABLE_ANCHOR_RETICLE_RENDERING" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(90);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SHOW_ANCHOR_LINE" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(91);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "FREE_SPIDER_REFLEXES" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(92);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SHOW_BAR_OF_SHAME" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(93);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SHOW_ENEMY_HEALTH_WIDGETS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(94);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "ALLOW_IGC_PAUSE" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(95);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SHOW_OBBS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(96);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SHOW_DISTRICTS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(97);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SHOW_CHUCK_DEBUGGER" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(98);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "CHUCK_OLD_FASHIONED" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(99);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "CHUCK_DISABLE_BREAKPOINTS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(100);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SHOW_AUDIO_BOXES" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(101);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "DISABLE_SOUNDS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(102);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SHOW_TERRAIN_INFO" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(103);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "DISABLE_AUDIO_BOXES" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(104);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "NSL_OLD_FASHIONED" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(105);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SHOW_MASTER_CLOCK" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(106);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "LOAD_GRADIENTS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(107);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "BONUS_LEVELS_AVAILABLE" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(108);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "COMBAT_DISPLAY" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(109);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "COMBAT_DEBUGGER" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(110);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "ALLOW_ERROR_POPUPS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(111);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "ALLOW_WARNING_POPUPS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(112);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "OUTPUT_WARNING_DISABLE" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(113);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "OUTPUT_ASSERT_DISABLE" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(114);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "ASSERT_ON_WARNING" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(115);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "ALWAYS_ACTIVE" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(116);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "FORCE_PROGRESSION" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(117);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "LINK" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(118);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "WAIT_FOR_LINK" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(119);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SHOW_END_OF_PACK" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(120);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        BYTE* v95 = *(BYTE**)0x0096858C;
        debug_menu_entry v96 = { "LIVE_IN_GLASS_HOUSE", POINTER_BOOL, &v95[4 + 0x7A] };
        add_debug_menu_entry(devopt_flags_menu, &v96);

        v90 = debug_menu_entry { mString { "SHOW_GLASS_HOUSE" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(122);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "DISABLE_RACE_PREVIEW" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(123);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "FREE_MINI_MAP" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(124);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SHOW_LOOPING_ANIM_WARNINGS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(125);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SHOW_PERF_INFO" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(126);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "COPY_ERROR_TO_CLIPBOARD" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(127);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "BOTH_HANDS_UP_REORIENT" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(128);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "SHOW_CAMERA_PROJECTION" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(129);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "OLD_DEFAULT_CONTROL_SETTINGS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(130);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "IGC_SPEED_CONTROL" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(131);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "RTDT_ENABLED" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(132);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "ENABLE_DECALS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(133);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "AUTO_STICK_TO_WALLS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(134);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "ENABLE_PEDESTRIANS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(135);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "CAMERA_INVERT_LOOKAROUND" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(136);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "CAMERA_INVERT_LOOKAROUND_X" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(137);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "CAMERA_INVERT_LOOKAROUND_Y" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(138);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "FORCE_COMBAT_CAMERA_OFF" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(139);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "DISPLAY_THOUGHT_BUBBLES" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(140);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "ENABLE_DEBUG_LOG" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(141);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "ENABLE_LONG_CALLSTACK" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(142);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "RENDER_FE_UI" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(147);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "LOCK_INTERIORS" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(144);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "MEMCHECK_ON_LOAD" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(145);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "DISPLAY_ALS_USAGE_PROFILE" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(146);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        v90 = debug_menu_entry { mString { "ENABLE_FPU_EXCEPTION_HANDLING" } };
        v90.set_bval(false);
        v90.set_game_flags_handler(devopt_flags_handler);
        v90.set_id(143);
        add_debug_menu_entry(devopt_flags_menu, &v90);

        BYTE* v93 = *(BYTE**)0x0096858C;
        debug_menu_entry v94 = { "UNLOCK_ALL_UNLOCKABLES", POINTER_BOOL, &v93[4 + 0x95] };
        add_debug_menu_entry(devopt_flags_menu, &v94);
}

void create_devopt_int_menu(debug_menu * parent)
        {
        assert(parent != nullptr);

        auto* v22 = create_menu("Devopt Ints", handle_devopt_entry, 300);

        for (auto idx = 0u; idx < NUM_OPTIONS; ++idx) {
            auto* v21 = get_option(idx);
            switch (v21->m_type) {
            case game_option_t::INT_OPTION: {
                                auto v20 = debug_menu_entry(mString { v21->m_name });
                                v20.set_p_ival(v21->m_value.p_ival);
                                v20.set_min_value(-1000.0);
                                v20.set_max_value(1000.0);
                                v22->add_entry(&v20);
                                break;
            }
            default:
                                break;
            }
        }

        auto v5 = debug_menu_entry(v22);
        parent->add_entry(&v5);
        }
  






namespace spider_monkey {
        bool is_running()
        {
            return (bool)CDECL_CALL(0x004B3B60);
        }
        }

        void tick()
        {

            CDECL_CALL(0x005D6FC0);
        }

        static inline debug_menu* root_menu = nullptr;

        void grab_focus()
        {
            physics_state_on_exit = !g_game_ptr()->is_physics_enabled();
            g_game_ptr()->enable_physics(false);
            has_focus = true;
        }

        void show()
        {
            root_menu = debug_menu::root_menu;
            grab_focus();
        }

        static void hide()
        {
            close_debug();
        }


void game_flags_handler(debug_menu_entry *a1)
{
    switch ( a1->get_id() )
    {
    case 0u: //Physics Enabled
    {
        auto v1 = a1->get_bval();
        g_game_ptr()->enable_physics(v1);
        debug_menu::physics_state_on_exit = a1->get_bval();
        break;
    }
    case 1u: //Single Step
    {
        g_game_ptr()->flag.single_step = true;
        break;
    }
    case 2u: //Slow Motion Enabled
    
        {
                                static int old_frame_lock = 0;
                                int v27;
                                if (a1->get_bval()) {
                                old_frame_lock = os_developer_options::instance()->get_int(mString { "FRAME_LOCK" });
                                v27 = 120;
                                } else {
                                v27 = old_frame_lock;
                                }

                                os_developer_options::instance()->set_int(mString { "FRAME_LOCK" }, v27);
                                debug_menu::hide();
                                break;
        }
    case 3u: //Monkey Enabled
    {
        if ( a1->get_bval() )
        {
            spider_monkey::start();
            spider_monkey::on_level_load();
            auto *v2 = input_mgr::instance();
            auto *rumble_device = v2->rumble_ptr;

            assert(rumble_device != nullptr);
            rumble_device->disable_vibration();
        }
        else
        {
            spider_monkey::on_level_unload();
            spider_monkey::stop();
        }

        debug_menu::hide();
        break;
    }
    case 4u:
    {
        auto *v3 = input_mgr::instance();
        auto *rumble_device = v3->rumble_ptr;
        assert(rumble_device != nullptr);

        if ( a1->get_bval() )
            rumble_device->enable_vibration();
        else
            rumble_device->disable_vibration();

        break;
    }
    case 5u: //God Mode
    {
        auto v4 = a1->get_ival3();
        set_god_mode(v4);
        debug_menu::hide();
        break;
    }
    case 6u: //Show Districts
    {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString{"SHOW_STREAMER_INFO"}, a1->get_bval());

        if ( a1->get_bval() )
        {
            os_developer_options::instance()->set_flag(mString{"SHOW_DEBUG_TEXT"}, true);
        }

        //TODO
        //sub_66C242(&g_game_ptr->field_4C);
        break;
    }
    case 7u: //Show Hero Position
    {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString{"SHOW_DEBUG_INFO"}, a1->get_bval());
        break;
    }
    case 8u:
    {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString{"SHOW_FPS"}, a1->get_bval());
        break;
    }
    case 9u:
    {
        auto v24 = a1->get_bval();
        auto *v5 = input_mgr::instance();
        if ( !v5->field_30[1] )
        {
            v24 = false;
        }


        os_developer_options::instance()->set_flag(mString{"USERCAM_ON_CONTROLLER2"}, v24);
        auto *v6 = input_mgr::instance();
        auto *v23 = v6->field_30[1];

        //TODO
        /*
        if ( !(*(unsigned __int8 (__thiscall **)(int))(*(_DWORD *)v23 + 44))(v23) )
        {
            j_debug_print_va("Controller 2 is not connected!");
            ->set_bval(a1, 0, 1);
            v24 = 0;
        }
        if ( v24 )
        {
            j_debug_print_va("User cam (theoretically) enabled on controller 2");
            v7 = (*(int (__thiscall **)(int))(*(_DWORD *)v23 + 8))(v23);
            sub_676E45(g_mouselook_controller, v7);
        }
        else
        {
            sub_676E45(g_mouselook_controller, -1);
        }
        */

        auto *v8 = g_world_ptr()->get_hero_ptr(0);
        if (v8 != nullptr && g_game_ptr()->m_user_camera_enabled)
        {
            if ( a1->get_bval() )
            {
                auto *v14 = g_world_ptr()->get_hero_ptr(0);
                v14->unsuspend(1);
            }
            else
            {
                auto *v15 = g_world_ptr()->get_hero_ptr(0);
                v15->suspend(1);
            }
        }
        break;
    }
    case 10u: // Invert Camera Look
    {
        debug_menu::hide();
        os_developer_options::instance()->set_flag(mString { "CAMERA_INVERT_LOOKAROUND" }, a1->get_bval());
        break;
    }
    case 11u: //Hires Screenshot
    {
        debug_menu::hide();
        auto a2 = os_developer_options::instance()->get_int(mString{"HIRES_SCREENSHOT_X"});
        auto a3 = os_developer_options::instance()->get_int(mString{"HIRES_SCREENSHOT_Y"});
        assert(a2 != 0 && a3 != 0 && "HIRES_SCREENSHOT_X and HIRES_SCREENSHOT_Y must be not equal 0");
        g_game_ptr()->begin_hires_screenshot(a2, a3);
        break;
    }
    case 12u: //Lores Screenshot
    {
        g_game_ptr()->push_lores();
        break;
    }
    case 13u:
    {
        static auto load_districts = TRUE;
        if ( load_districts )
        {
            auto *v11 = g_world_ptr()->the_terrain;
            v11->unload_all_districts_immediate();
            resource_manager::set_active_district(false);
        }
        else
        {
            resource_manager::set_active_district(true);
        }

        load_districts = !load_districts;
        debug_menu::hide();
        break;
    }
    case 14u:
    {
        //TODO
        //sub_66FBE0();
        debug_menu::hide();
        break;
    }
    case 15u:
    {
        //sub_697DB1();
        debug_menu::hide();
        break;
    }
    case 16u:
    {
        //TODO
        //sub_698D33();
        debug_menu::hide();
        break;
    }
    case 17u:
    {
        [[maybe_unused]]auto v12 = a1->get_bval();

        //TODO
        //sub_6A88A5(g_game_ptr, v12);
        break;
    }
    case 18u:
    {
        auto v13 = a1->get_ival();
        a1->set_ival(v13, 0);
        auto v16 = a1->get_ival();
        if ( v16 )
        {
            if ( v16 == 1 )
            {
                if ( geometry_manager::is_scene_analyzer_enabled() )
                {
                    geometry_manager::enable_scene_analyzer(false);
                }
                if (g_game_ptr()->is_user_camera_enabled()) {
                    g_game_ptr()->m_user_camera_enabled = true;
                }
 

            }
            else if ( v16 == 2 )
            {
                g_game_ptr()->m_user_camera_enabled = false;
                geometry_manager::enable_scene_analyzer(true);
            }
        }
        else
        {
            if ( geometry_manager::is_scene_analyzer_enabled() )
            {
                geometry_manager::enable_scene_analyzer(false);
            }

            g_game_ptr()->m_user_camera_enabled = false;
        }
    break;
    }
    default:
    return;
    }
}

void handle_extra_entry(debug_menu_entry* entry)
{

BYTE* val = (BYTE*)entry->data;
    *val = !*val;
}



void create_game_menu(debug_menu* parent)
{


    auto* game_menu = create_menu("Game", debug_menu::sort_mode_t::undefined);
    auto* v0 = create_menu_entry(game_menu);
    parent->add_entry(v0);

    create_devopt_flags_menu(game_menu);
    create_devopt_int_menu(game_menu);
    create_gamefile_menu(game_menu);





    auto* extra_menu = create_menu("Extra Options", (menu_handler_function)handle_extra_entry, 10);
    auto* v15 = create_menu_entry(extra_menu);
    add_debug_menu_entry(game_menu, v15);



       debug_menu_entry time_of_day (mString { "Time of Day" });
    const float v5[4] = { -1, 2, 1, 1 };
    time_of_day.set_fl_values(v5);
        time_of_day.entry_type = INTEGER;
        time_of_day.custom_string_generator = time_of_day_name_generator;
        time_of_day.custom_handler = time_of_day_handler;
        time_of_day.data = (void*)0xFFFFFFFF;
        add_debug_menu_entry(extra_menu, &time_of_day);

    debug_menu_entry monkeytime = { "Monkey Time", POINTER_BOOL, (void*)0x959E60 };
    add_debug_menu_entry(extra_menu, &monkeytime);
    debug_menu_entry unlockchars = { "Unlock All Characters and Covers", POINTER_BOOL, (void*)0x960E1C };
    add_debug_menu_entry(extra_menu, &unlockchars);
    debug_menu_entry unlockalllandmarks = { "Unlock All Landmarks", POINTER_BOOL, (void*)0x960E19 };
    add_debug_menu_entry(extra_menu, &unlockalllandmarks);
    debug_menu_entry unlockallconceptart = { "Unlock All Concept Art", POINTER_BOOL, (void*)0x960E1A };
    add_debug_menu_entry(extra_menu, &unlockallconceptart);
    debug_menu_entry unlockallcovers = { "Unlock All Covers", POINTER_BOOL, (void*)0x960E1D };
    add_debug_menu_entry(extra_menu, &unlockallcovers);
    debug_menu_entry disablepausemenu = { "Disable Pause Menu", POINTER_BOOL, (void*)0x96B448 };
    add_debug_menu_entry(extra_menu, &disablepausemenu);

    auto* entry = create_menu_entry(mString { "Report SLF Recall Timeouts" });
    static bool byte_1597BC0 = false;
    entry->set_pt_bval(&byte_1597BC0);
    game_menu->add_entry(entry);

    auto* entry1 = create_menu_entry(mString { "Physics Enabled" });
    entry1->set_bval(true);
    entry1->set_game_flags_handler(game_flags_handler);
    entry1->set_id(0);
    game_menu->add_entry(entry1);


    auto* entry2 = create_menu_entry(mString { "Single Step" });
    entry2->set_game_flags_handler(game_flags_handler);
    entry2->set_id(1);
    game_menu->add_entry(entry2);

    auto* entry3 = create_menu_entry(mString { "Slow Motion Enabled" });
    entry3->set_bval(false);
    entry3->set_game_flags_handler(game_flags_handler);
    entry3->set_id(2);
    game_menu->add_entry(entry3);

    auto* entry4 = create_menu_entry(mString { "Monkey Enabled" });
    auto v1 = spider_monkey::is_running();
    entry4->set_bval(false);
    entry4->set_game_flags_handler(game_flags_handler);
    entry4->set_id(3);
    game_menu->add_entry(entry4);

    auto* entry5 = create_menu_entry(mString { "Rumble Enabled" });
    entry5->set_bval(true);
    entry5->set_game_flags_handler(game_flags_handler);
    entry5->set_id(4);
    game_menu->add_entry(entry5);


    auto* entry6 = create_menu_entry(mString { "God Mode" });
    entry6->set_ival3(os_developer_options::instance()->get_int(mString { "GOD_MODE" }));
    const float v2[4] = { 0, 5, 1, 1 };
    entry6->set_fl_values(v2);
    entry6->set_game_flags_handler(game_flags_handler);
    entry6->set_id(5);
    game_menu->add_entry(entry6);


    auto* entry7 = create_menu_entry(mString { "Show Districts" });
    entry7->set_bval(os_developer_options::instance()->get_flag(mString { "SHOW_STREAMER_INFO" }));
    entry7->set_game_flags_handler(game_flags_handler);
    entry7->set_id(6);
    game_menu->add_entry(entry7);

    
    auto* entry8 = create_menu_entry(mString { "Show Hero Position" });
    entry8->set_bval(os_developer_options::instance()->get_flag(mString { "SHOW_DEBUG_INFO" }));
    entry8->set_bval(false);
    entry8->set_game_flags_handler(game_flags_handler);
    entry8->set_id(7);
    game_menu->add_entry(entry8);

    auto* entry9 = create_menu_entry(mString { "Show FPS" });
    entry9->set_bval(os_developer_options::instance()->get_flag(mString { "SHOW_FPS" }));
    entry9->set_bval(false);
    entry9->set_game_flags_handler(game_flags_handler);
    entry9->set_id(8);
    game_menu->add_entry(entry9);


    auto* entry10 = create_menu_entry(mString { "User Camera on Controller 2" });
    entry10->set_bval(os_developer_options::instance()->get_flag(mString { "USERCAM_ON_CONTROLLER2" }));
    entry10->set_game_flags_handler(game_flags_handler);
    entry10->set_id(9);
    game_menu->add_entry(entry10);

    auto* entry17 = create_menu_entry(mString { "Invert Camera Look" });
    entry17->set_bval(os_developer_options::instance()->get_flag(mString { "CAMERA_INVERT_LOOKAROUND" }));
    entry17->set_game_flags_handler(game_flags_handler);
    entry17->set_id(10);
    game_menu->add_entry(entry17);


    auto* entry13 = create_menu_entry(mString { "Toggle Unload All Districts" });
    entry13->set_game_flags_handler(game_flags_handler);
    entry13->set_id(13);
    game_menu->add_entry(entry13);



    auto* entry14 = create_menu_entry(mString { "Save Game" });
    entry14->set_game_flags_handler(game_flags_handler);
    entry14->set_id(14);
    game_menu->add_entry(entry14);

    auto* entry15 = create_menu_entry(mString { "Load Game" });
    entry15->set_game_flags_handler(game_flags_handler);
    entry15->set_id(15);
    game_menu->add_entry(entry15);

    auto* entry16 = create_menu_entry(mString { "Attemp Auto Load" });
    entry16->set_game_flags_handler(game_flags_handler);
    entry16->set_id(16);
    game_menu->add_entry(entry16);



    auto* entry11 = create_menu_entry(mString { "Lores Screenshot" });
    entry11->set_game_flags_handler(game_flags_handler);
    entry11->set_id(11);
    game_menu->add_entry(entry11);

    auto* entry12 = create_menu_entry(mString { "Hires Screenshot" });
    entry12->set_game_flags_handler(game_flags_handler);
    entry12->set_id(12);
    game_menu->add_entry(entry12);
}






        bool is_user_camera_enabled()
{

    g_game_ptr()->m_user_camera_enabled = true;
    return 1;
}

void debug_flags_handler(debug_menu_entry* entry)
{
    switch (entry->get_id()) {
    case 0u: // Camera State
    {
        auto v13 = entry->get_ival2();
        entry->set_ival2(v13, false);
        auto v16 = entry->get_ival2();
        if (v16 != 0) {
            if (v16 == 1) {
                if (geometry_manager::is_scene_analyzer_enabled()) {
                    geometry_manager::enable_scene_analyzer(false);
                }
             if (spider_monkey::is_running() || !os_developer_options::instance()->get_flag("ENABLE_ZOOM_MAP") || g_game_ptr()->is_physics_enabled()) {
                show();
            }
                is_user_camera_enabled();

                

            } else if (v16 == 2) {
                g_game_ptr()->m_user_camera_enabled = false;
                geometry_manager::enable_scene_analyzer(true);
            }
        } else {
            if (geometry_manager::is_scene_analyzer_enabled()) {
                geometry_manager::enable_scene_analyzer(false);
            }

            g_game_ptr()->m_user_camera_enabled = false;
        }
    }
    }
}







void create_ai_menu(debug_menu* parent)
{
    auto* ai_menu = create_menu("AI", debug_menu::sort_mode_t::undefined);
    auto* v1 = create_menu_entry(ai_menu);
    parent->add_entry(v1);

    auto* ai_menu0 = create_menu("0x0003ac24e", debug_menu::sort_mode_t::undefined);
    auto* v0 = create_menu_entry(ai_menu0);
    ai_menu->add_entry(v0);

    auto* ai_menu19 = create_menu("-Core params", debug_menu::sort_mode_t::undefined);
    auto* v20 = create_menu_entry(ai_menu19);
    ai_menu0->add_entry(v20);

    auto* entry25 = create_menu_entry(mString { "--Export this block--" });
    ai_menu19->add_entry(entry25);

    auto* entry26 = create_menu_entry(mString { "0x00415687 : 0x00415687 0x00415687 (hash)" });
    ai_menu19->add_entry(entry26);

    auto* entry27 = create_menu_entry(mString { "0x02b1b1ad" });
    static float _0x02b1b1ad = 12.00;
    entry27->set_pt_fval(&_0x02b1b1ad);
    entry27->set_max_value(1000.0);
    entry27->set_min_value(-1000.0);
    ai_menu19->add_entry(entry27);

    auto* entry28 = create_menu_entry(mString { "0x055682a0" });
    static float _0x055682a0 = 6.00;
    entry28->set_pt_fval(&_0x055682a0);
    entry28->set_max_value(1000.0);
    entry28->set_min_value(-1000.0);
    ai_menu19->add_entry(entry28);

    auto* entry29 = create_menu_entry(mString { "0x0674abb8" });
    static float _0x0674abb8 = 30.00;
    entry29->set_pt_fval(&_0x0674abb8);
    entry29->set_max_value(1000.0);
    entry29->set_min_value(-1000.0);
    ai_menu19->add_entry(entry29);

    auto* entry30 = create_menu_entry(mString { "0x0848ac5d" });
    static float _0x0848ac5d = 0.00;
    entry30->set_pt_fval(&_0x0848ac5d);
    entry30->set_max_value(1000.0);
    entry30->set_min_value(-1000.0);
    ai_menu19->add_entry(entry30);

    auto* entry31 = create_menu_entry(mString { "0x08e1b441" });
    static int _0x08e1b441 = 0;
    entry31->set_p_ival(&_0x08e1b441);
    ai_menu19->add_entry(entry31);

    auto* entry32 = create_menu_entry(mString { "0x0a0d1f8d" });
    static int _0x0a0d1f8d = 1;
    entry32->set_p_ival(&_0x0a0d1f8d);
    ai_menu19->add_entry(entry32);

    auto* entry33 = create_menu_entry(mString { "0x0d1e4da9" });
    static float _0x0d1e4da9 = 3.50;
    entry33->set_pt_fval(&_0x0d1e4da9);
    entry33->set_max_value(1000.0);
    entry33->set_min_value(-1000.0);
    ai_menu19->add_entry(entry33);

    auto* entry34 = create_menu_entry(mString { "0x10c22073" });
    static float _0x10c22073 = 20.00;
    entry34->set_pt_fval(&_0x10c22073);
    entry34->set_max_value(1000.0);
    entry34->set_min_value(-1000.0);
    ai_menu19->add_entry(entry34);


    auto* entry35 = create_menu_entry(mString { "0x12d1c201" });
    static float _0x12d1c201 = 90.00;
    entry35->set_pt_fval(&_0x12d1c201);
    entry35->set_max_value(1000.0);
    entry35->set_min_value(-1000.0);
    ai_menu19->add_entry(entry35);

    auto* entry36 = create_menu_entry(mString { "0x1800362a" });
    static float _0x1800362a = 6.00;
    entry36->set_pt_fval(&_0x1800362a);
    entry36->set_max_value(1000.0);
    entry36->set_min_value(-1000.0);
    ai_menu19->add_entry(entry36);

    auto* entry37 = create_menu_entry(mString { "0x183ca8db" });
    static float _0x183ca8db = 60.00;
    entry37->set_pt_fval(&_0x183ca8db);
    entry37->set_max_value(1000.0);
    entry37->set_min_value(-1000.0);
    ai_menu19->add_entry(entry37);


    auto* ai_menu1 = create_menu("0x0001ab00 inode params", debug_menu::sort_mode_t::undefined);
    auto* v2 = create_menu_entry(ai_menu1);
    ai_menu0->add_entry(v2);

    auto* entry23 = create_menu_entry(mString { "--None defined--" });
    ai_menu1->add_entry(entry23);

    auto* ai_menu2 = create_menu("0x003ac24e inode params", debug_menu::sort_mode_t::undefined);
    auto* v3 = create_menu_entry(ai_menu2);
    ai_menu0->add_entry(v3);

    auto* entry = create_menu_entry(mString { "--None defined--" });
    ai_menu2->add_entry(entry);

    auto* ai_menu3 = create_menu("0x08641048 inode params", debug_menu::sort_mode_t::undefined);
    auto* v4 = create_menu_entry(ai_menu3);
    ai_menu0->add_entry(v4);

    auto* entry2 = create_menu_entry(mString { "--None defined--" });
    ai_menu3->add_entry(entry2);

    auto* ai_menu4 = create_menu("0x15897c0c inode params", debug_menu::sort_mode_t::undefined);
    auto* v5 = create_menu_entry(ai_menu4);
    ai_menu0->add_entry(v5);

    auto* entry21 = create_menu_entry(mString { "--Export this block--" });
    ai_menu4->add_entry(entry21);

    auto* entry22 = create_menu_entry(mString { "0x66a4c480" });
    entry22->set_ival(0);
    entry22->set_max_value(1000.0);
    entry22->set_min_value(-1000.0);
    ai_menu4->add_entry(entry22);

    auto* entry24 = create_menu_entry(mString { "0x81b9a503 : 0x81b9a503 0x81b9a503 (hash)" });
    ai_menu4->add_entry(entry24);

    auto* ai_menu5 = create_menu("0x1754b0dc inode params", debug_menu::sort_mode_t::undefined);
    auto* v6 = create_menu_entry(ai_menu5);
    ai_menu0->add_entry(v6);

    auto* entry3 = create_menu_entry(mString { "--None defined--" });
    ai_menu5->add_entry(entry3);

    auto* ai_menu6 = create_menu("0x1b17cb5d inode params", debug_menu::sort_mode_t::undefined);
    auto* v7 = create_menu_entry(ai_menu6);
    ai_menu0->add_entry(v7);

    auto* entry4 = create_menu_entry(mString { "--None defined--" });
    ai_menu6->add_entry(entry4);

    auto* ai_menu7 = create_menu("0x1cf15fd1 inode params", debug_menu::sort_mode_t::undefined);
    auto* v8 = create_menu_entry(ai_menu7);
    ai_menu0->add_entry(v8);

    auto* entry8 = create_menu_entry(mString { "--None defined--" });
    ai_menu7->add_entry(entry8);

    auto* ai_menu8 = create_menu("0x371268f7 inode params", debug_menu::sort_mode_t::undefined);
    auto* v9 = create_menu_entry(ai_menu8);
    ai_menu0->add_entry(v9);

    auto* entry9 = create_menu_entry(mString { "--None defined--" });
    ai_menu8->add_entry(entry9);

    auto* ai_menu9 = create_menu("0x5d0c49a4 inode params", debug_menu::sort_mode_t::undefined);
    auto* v10 = create_menu_entry(ai_menu9);
    ai_menu0->add_entry(v10);

    auto* entry10 = create_menu_entry(mString { "--None defined--" });
    ai_menu9->add_entry(entry10);

    auto* ai_menu10 = create_menu("0x94b51e64 inode params", debug_menu::sort_mode_t::undefined);
    auto* v11 = create_menu_entry(ai_menu10);
    ai_menu0->add_entry(v11);

    auto* entry11 = create_menu_entry(mString { "--None defined--" });
    ai_menu10->add_entry(entry11);

    auto* ai_menu11 = create_menu("0x9ee13b40 inode params", debug_menu::sort_mode_t::undefined);
    auto* v12 = create_menu_entry(ai_menu11);
    ai_menu0->add_entry(v12);

    auto* entry12 = create_menu_entry(mString { "--None defined--" });
    ai_menu11->add_entry(entry12);

    auto* ai_menu12 = create_menu("0xa2d277fe inode params", debug_menu::sort_mode_t::undefined);
    auto* v13 = create_menu_entry(ai_menu12);
    ai_menu0->add_entry(v13);

    auto* entry13 = create_menu_entry(mString { "--Export this block--" });
    ai_menu12->add_entry(entry13);

    auto* entry14 = create_menu_entry(mString { "0x28f66f97 : 0x28f66f97 SPI (fixedstring)" });
    ai_menu12->add_entry(entry14);

    auto* ai_menu13 = create_menu("0xa8e18643 inode params", debug_menu::sort_mode_t::undefined);
    auto* v14 = create_menu_entry(ai_menu13);
    ai_menu0->add_entry(v14);

    auto* entry15 = create_menu_entry(mString { "--None defined--" });
    ai_menu13->add_entry(entry15);

    auto* ai_menu14 = create_menu("0xc8553c6e inode params", debug_menu::sort_mode_t::undefined);
    auto* v15 = create_menu_entry(ai_menu14);
    ai_menu0->add_entry(v15);

    auto* entry16 = create_menu_entry(mString { "--None defined--" });
    ai_menu14->add_entry(entry16);

    auto* ai_menu15 = create_menu("0xcc62c392 inode params", debug_menu::sort_mode_t::undefined);
    auto* v16 = create_menu_entry(ai_menu15);
    ai_menu0->add_entry(v16);

    auto* entry17 = create_menu_entry(mString { "--None defined--" });
    ai_menu15->add_entry(entry17);

    auto* ai_menu16 = create_menu("0xccf57218 inode params", debug_menu::sort_mode_t::undefined);
    auto* v17 = create_menu_entry(ai_menu16);
    ai_menu0->add_entry(v17);

    auto* entry18 = create_menu_entry(mString { "--None defined--" });
    ai_menu16->add_entry(entry18);

    auto* ai_menu17 = create_menu("0xd552ba6d inode params", debug_menu::sort_mode_t::undefined);
    auto* v18 = create_menu_entry(ai_menu17);
    ai_menu0->add_entry(v18);

    auto* entry19 = create_menu_entry(mString { "--None defined--" });
    ai_menu17->add_entry(entry19);

    auto* ai_menu18 = create_menu("0xd664a286 inode params", debug_menu::sort_mode_t::undefined);
    auto* v19 = create_menu_entry(ai_menu18);
    ai_menu0->add_entry(v19);

    auto* entry20 = create_menu_entry(mString { "--None defined--" });
    ai_menu18->add_entry(entry20);

}


void create_entity_variants_menu(debug_menu* parent)
{
    auto* entity_variants_menu = create_menu("Entity Variants", debug_menu::sort_mode_t::undefined);
    auto* v1 = create_menu_entry(entity_variants_menu);
    parent->add_entry(v1);

    auto* entity_variants_menu0 = create_menu("0x00823d088", debug_menu::sort_mode_t::undefined);
    auto* v0 = create_menu_entry(entity_variants_menu0);
    entity_variants_menu->add_entry(v0);

    auto* entry = create_menu_entry(mString { "0xeb61a603" });
    entity_variants_menu0->add_entry(entry);

    auto* entry1 = create_menu_entry(mString { "0x98e9ec31" });
    entity_variants_menu0->add_entry(entry1);

    auto* entry2 = create_menu_entry(mString { "0x98eb8450" });
    entity_variants_menu0->add_entry(entry2);

    auto* entry3 = create_menu_entry(mString { "0x6a16b8a9" });
    entity_variants_menu0->add_entry(entry3);

    auto* entry4 = create_menu_entry(mString { "0x6a1650c8" });
    entity_variants_menu0->add_entry(entry4);

    auto* entity_variants_menu1 = create_menu("0x00823d089", debug_menu::sort_mode_t::undefined);
    auto* v3 = create_menu_entry(entity_variants_menu1);
    entity_variants_menu->add_entry(v3);

    auto* entry5 = create_menu_entry(mString { "0xeb61a303" });
    entity_variants_menu1->add_entry(entry5);

    auto* entry6 = create_menu_entry(mString { "0x435bbbac" });
    entity_variants_menu1->add_entry(entry6);

    auto* entry7 = create_menu_entry(mString { "0x435f580f" });
    entity_variants_menu1->add_entry(entry7);

    auto* entity_variants_menu2 = create_menu("0x00823d08a", debug_menu::sort_mode_t::undefined);
    auto* v4 = create_menu_entry(entity_variants_menu2);
    entity_variants_menu->add_entry(v4);

    auto* entry8 = create_menu_entry(mString { "0xeb61a303" });
    entity_variants_menu2->add_entry(entry8);

    auto* entry9 = create_menu_entry(mString { "0x435bbbac" });
    entity_variants_menu2->add_entry(entry9);

    auto* entry10 = create_menu_entry(mString { "0x435f580f" });
    entity_variants_menu2->add_entry(entry10);

    auto* entity_variants_menu3 = create_menu("0x00823d08b", debug_menu::sort_mode_t::undefined);
    auto* v5 = create_menu_entry(entity_variants_menu3);
    entity_variants_menu->add_entry(v5);

    auto* entry11 = create_menu_entry(mString { "0xeb61a303" });
    entity_variants_menu3->add_entry(entry11);

    auto* entry12 = create_menu_entry(mString { "0x435bbbac" });
    entity_variants_menu3->add_entry(entry12);

    auto* entry13 = create_menu_entry(mString { "0x435f580f" });
    entity_variants_menu3->add_entry(entry13);

    auto* entity_variants_menu4 = create_menu("0x00823d08c", debug_menu::sort_mode_t::undefined);
    auto* v6 = create_menu_entry(entity_variants_menu4);
    entity_variants_menu->add_entry(v6);

    auto* entry14 = create_menu_entry(mString { "0xeb61a603" });
    entity_variants_menu4->add_entry(entry14);

    auto* entry15 = create_menu_entry(mString { "0x98e9ec31" });
    entity_variants_menu4->add_entry(entry15);

    auto* entry16 = create_menu_entry(mString { "0x98eb8450" });
    entity_variants_menu4->add_entry(entry16);

    auto* entry17 = create_menu_entry(mString { "0x6a16b8a9" });
    entity_variants_menu4->add_entry(entry17);

    auto* entry18 = create_menu_entry(mString { "0x6a1650c8" });
    entity_variants_menu4->add_entry(entry18);

    auto* entity_variants_menu5 = create_menu("0x00823d08d", debug_menu::sort_mode_t::undefined);
    auto* v7 = create_menu_entry(entity_variants_menu5);
    entity_variants_menu->add_entry(v7);

    auto* entry19 = create_menu_entry(mString { "0xeb61a303" });
    entity_variants_menu5->add_entry(entry19);

    auto* entry20 = create_menu_entry(mString { "0x435bbbac" });
    entity_variants_menu5->add_entry(entry20);

    auto* entry21 = create_menu_entry(mString { "0x435f580f" });
    entity_variants_menu5->add_entry(entry21);

    auto* entity_variants_menu6 = create_menu("0x00823d08e", debug_menu::sort_mode_t::undefined);
    auto* v8 = create_menu_entry(entity_variants_menu6);
    entity_variants_menu->add_entry(v8);

    auto* entry22 = create_menu_entry(mString { "0xeb61a603" });
    entity_variants_menu6->add_entry(entry22);

    auto* entry23 = create_menu_entry(mString { "0x98e9ec31" });
    entity_variants_menu6->add_entry(entry23);

    auto* entry24 = create_menu_entry(mString { "0x98eb8450" });
    entity_variants_menu6->add_entry(entry24);

    auto* entry25 = create_menu_entry(mString { "0x6a16b8a9" });
    entity_variants_menu6->add_entry(entry25);

    auto* entry26 = create_menu_entry(mString { "0x6a1650c8" });
    entity_variants_menu6->add_entry(entry26);

    auto* entity_variants_menu7 = create_menu("0x00823d08f", debug_menu::sort_mode_t::undefined);
    auto* v9 = create_menu_entry(entity_variants_menu7);
    entity_variants_menu->add_entry(v9);

    auto* entry27 = create_menu_entry(mString { "0xeb61a303" });
    entity_variants_menu7->add_entry(entry27);

    auto* entry28 = create_menu_entry(mString { "0x435bbbac" });
    entity_variants_menu7->add_entry(entry28);

    auto* entry29 = create_menu_entry(mString { "0x435f580f" });
    entity_variants_menu7->add_entry(entry29);


    auto* entity_variants_menu8 = create_menu("0x00823d090", debug_menu::sort_mode_t::undefined);
    auto* v10 = create_menu_entry(entity_variants_menu8);
    entity_variants_menu->add_entry(v10);

    auto* entry30 = create_menu_entry(mString { "0xeb61a603" });
    entity_variants_menu8->add_entry(entry30);

    auto* entry31 = create_menu_entry(mString { "0x98e9ec31" });
    entity_variants_menu8->add_entry(entry31);

    auto* entry32 = create_menu_entry(mString { "0x98eb8450" });
    entity_variants_menu8->add_entry(entry32);

    auto* entry33 = create_menu_entry(mString { "0x6a16b8a9" });
    entity_variants_menu8->add_entry(entry33);

    auto* entry34 = create_menu_entry(mString { "0x6a1650c8" });
    entity_variants_menu8->add_entry(entry34);

    auto* entity_variants_menu9 = create_menu("0x00823d091", debug_menu::sort_mode_t::undefined);
    auto* v11 = create_menu_entry(entity_variants_menu9);
    entity_variants_menu->add_entry(v11);

    auto* entry35 = create_menu_entry(mString { "0xeb61a603" });
    entity_variants_menu9->add_entry(entry35);

    auto* entry36 = create_menu_entry(mString { "0x98e9ec31" });
    entity_variants_menu9->add_entry(entry36);

    auto* entry37 = create_menu_entry(mString { "0x98eb8450" });
    entity_variants_menu9->add_entry(entry37);

    auto* entry38 = create_menu_entry(mString { "0x6a16b8a9" });
    entity_variants_menu9->add_entry(entry38);

    auto* entry39 = create_menu_entry(mString { "0x6a1650c8" });
    entity_variants_menu9->add_entry(entry39);



}



void create_camera_menu_items(debug_menu* parent);



void create_camera_menu_items(debug_menu* parent)
{
    assert(parent != nullptr);

    auto* new_menu_entry = create_menu_entry(mString { "Camera" });

    float v1[4] = { 0, 2, 1, 1 };
    new_menu_entry->set_fl_values(v1);
    new_menu_entry->set_game_flags_handler(debug_flags_handler);
    new_menu_entry->set_ival2(0);
    parent->add_entry(new_menu_entry);
    g_debug_camera_entry = new_menu_entry;
}




void replay_handler(debug_menu_entry* entry)
{
   auto result = entry->field_E;
    if (!entry->field_E) {
       debug_menu::root_menu = 0;
        had_menu_this_frame = 1;
    }
    debug_menu::hide();
}



void populate_replay_menu(debug_menu_entry* entry)
{
    // assert(parent != nullptr);

    auto* head_menu = create_menu("Replay", debug_menu::sort_mode_t::ascending);
    entry->set_submenu(head_menu);

    mString v25 { "Start" };
    debug_menu_entry v38 { v25.c_str() };

    v38.set_game_flags_handler(replay_handler);

    head_menu->add_entry(&v38);
}

void create_replay_menu(debug_menu* parent)
{
    auto* replay_menu = create_menu("Replay", debug_menu::sort_mode_t::undefined);
    auto* v2 = create_menu_entry(replay_menu);
    v2->set_game_flags_handler(populate_replay_menu);
    parent->add_entry(v2);
}

void create_progression_menu()
{
    progression_menu = create_menu("Progression", debug_menu::sort_mode_t::undefined);
    debug_menu_entry progression_entry { progression_menu };
    add_debug_menu_entry(debug_menu::root_menu, &progression_entry);
}

void create_script_menu()
{
    script_menu = create_menu("Script", (menu_handler_function)handle_script_select_entry, 50);
    debug_menu_entry script_entry { script_menu };
    add_debug_menu_entry(debug_menu::root_menu, &script_entry);
}


void debug_menu::init() {

    root_menu = create_menu("Debug Menu");






//    create_dvars_menu(root_menu);
    create_warp_menu(root_menu);
    create_game_menu(root_menu);
	create_missions_menu(root_menu);
    create_debug_render_menu(root_menu);
    create_debug_district_variants_menu(root_menu);
    create_replay_menu(root_menu);
    create_memory_menu(root_menu);
    create_ai_menu(root_menu);
    create_entity_variants_menu(root_menu);
    create_level_select_menu(root_menu);
    create_script_menu();
    create_progression_menu();



//    create_entity_animation_menu(root_menu);

    create_camera_menu_items(root_menu);

    





	/*
	for (int i = 0; i < 5; i++) {

		debug_menu_entry asdf;
		sprintf(asdf.text, "entry %d", i);
		printf("AQUI %s\n", asdf.text);

		add_debug_menu_entry(debug_menu::root_menu, &asdf);
	}
	add_debug_menu_entry(debug_menu::root_menu, &teste);
	*/
}



BOOL WINAPI DllMain(HINSTANCE hInstDll, DWORD fdwReason, LPVOID reserverd) {

	if (sizeof(region) != 0x134) {
		__debugbreak();
	}

	memset(keys, 0, sizeof(keys));
	if (fdwReason == DLL_PROCESS_ATTACH) {

#if 0
		AllocConsole();

		if (!freopen("CONOUT$", "w", stdout)) {
			MessageBoxA(NULL, "Error", "Couldn't allocate console...Closing", 0);
			return FALSE;
		}

#endif


		set_text_writeable();
		set_rdata_writeable();
		install_patches();

                hook_slf_vtables();
     

	}
	else if (fdwReason == DLL_PROCESS_DETACH)
		FreeConsole();

	return TRUE;
}

int main()
{
        return 0;
};
