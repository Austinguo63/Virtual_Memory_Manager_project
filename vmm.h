#ifndef VMM_H
#define VMM_H

#include "interface.h"

#include <ucontext.h>

// Declare your own data structures and functions here...

enum access_type{
    READ,
    WRITE,
};

struct PageInfo{
    int flags;
    int vm_page_index;
    int physical_frame_index;
    struct PageInfo *prev;
    struct PageInfo *next;
};

#define ACCESSED 0x01
#define MODIFIED 0x02
#define THIRD_CHANCE 0x04

extern void *vm_start_address;
extern enum policy_type eviction_policy;
extern size_t virtual_memory_size;
extern size_t memory_page_size;
extern size_t physical_frame_count;
extern size_t used_physical_frames;
extern struct PageInfo *current_page_info;

// the segmentation fault handler
void segv_handler(int sig, siginfo_t *info, void *ucontext);

enum access_type get_access_type(ucontext_t *ucontext_t);

int access_memory(int vm_page_index, int offset, enum access_type type);

struct PageInfo *find_page(int vm_page_index);

struct PageInfo *load_page(int vm_page_index, int offset, enum access_type type);

struct PageInfo *evict_page();

void * get_vm_page_address(int vm_page_index);

#endif
