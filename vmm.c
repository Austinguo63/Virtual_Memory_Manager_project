#include "vmm.h"
#include <assert.h>
// Memory Manager implementation
// Implement all other functions here...

void segv_handler(int sig, siginfo_t *info, void *ucontext){
    void * address = info->si_addr;
    enum access_type type = get_access_type(ucontext);

    if (address < vm_start_address || address >= vm_start_address + virtual_memory_size){
        // panic! segfault occurred outside the vm region! abort program
        printf("Segmentation fault on %s at %p\n", type == READ ? "read" : "write" ,address);
        exit(-1); 
    }

    size_t vm_offset = (address - vm_start_address);
    int vm_page_index = vm_offset / memory_page_size;
    int vm_page_offset = vm_offset % memory_page_size;

    printf("Trapped %s access to %p, vm page %d offset %d \n", type == READ ? "read" : "write", address, vm_page_index, vm_page_offset);
    
    int new_protection_flags = access_memory(vm_page_index, vm_page_offset, type);

    void *frame_start_address = (void *)((size_t)(info->si_addr) & ~(memory_page_size - 1));
    printf("New access control is %s%s\n", new_protection_flags & PROT_READ ? "READ" : "", new_protection_flags & PROT_WRITE ? "WRITE": "");
    mprotect(frame_start_address, memory_page_size, new_protection_flags);
}

// figure out the access type of the fault.
enum access_type get_access_type(ucontext_t *ucontext_t){
    // This is x86/64 specific, and will not work on other architectures.
    mcontext_t machine_context = ucontext_t->uc_mcontext;
    int error_code = machine_context.gregs[REG_ERR];
    // look at the second bit of the ERR register which should indicate
    // if the offending access is a read or a write.
    // https://github.com/torvalds/linux/blob/master/arch/x86/include/asm/trap_pf.h
    if ((error_code & 0x2) != 0) {
        return WRITE;
    } else {
        return READ;
    }
}

// loads the specifed page from memory
// evicting other pages if necessary
struct PageInfo *load_page(int vm_page_index, int offset, enum access_type type) {
    struct PageInfo *new_page_info;
    int evicted_page = -1;
    int write_back = 0;
    if (used_physical_frames < physical_frame_count) {
        // there is still space in physical memory
        // allocate a new page info struct
        new_page_info = malloc(sizeof(struct PageInfo));
        new_page_info->physical_frame_index = used_physical_frames;

        // now we insert this into the circular linked list
        if (current_page_info == NULL) {
            // list is empty
            new_page_info->next = new_page_info;
            new_page_info->prev = new_page_info;
        } else {
            // we insert the new page before the current page
            new_page_info->prev = current_page_info->prev;
            new_page_info->next = current_page_info;

            current_page_info->prev->next = new_page_info;
            current_page_info->prev = new_page_info;
        }

        current_page_info = new_page_info;
        used_physical_frames++;
    } else {
        // handle evictions here
        new_page_info = evict_page();
        evicted_page = new_page_info->vm_page_index;
        if (new_page_info->flags & MODIFIED) {
            // write back if the evicted page is modified
            write_back = 1;
        }
    }

    // mark the new page as accessed
    // as otherwise we will not be loading this into physical memory
    new_page_info->flags = ACCESSED;

    if (type == WRITE) {
        // mark as modified as well
        new_page_info->flags |= MODIFIED;
    }

    // set the address;
    new_page_info->vm_page_index = vm_page_index;

    // log this access
    mm_logger(vm_page_index, type == READ? 0 : 1, evicted_page, write_back, new_page_info->physical_frame_index * memory_page_size + offset);
    
    return new_page_info;
}

// finds a page to evict
struct PageInfo *evict_page(){
    switch(eviction_policy) {
        case MM_FIFO:
        current_page_info = current_page_info->prev;
        break;
        case MM_THIRD:
        current_page_info = current_page_info->prev;
        while (true) {
            // this should always brake eventually
            if (current_page_info->flags & ACCESSED){
                // page has been accessed
                // clear accessed flag and third chance flag
                current_page_info->flags &= (~ACCESSED);
                current_page_info->flags &= (~THIRD_CHANCE);
                // set protection to not allow any access
                // so future accesses are tracked.
                mprotect(get_vm_page_address(current_page_info->vm_page_index), memory_page_size, PROT_NONE);
            } else {
                if (!(current_page_info->flags & MODIFIED)) {
                    // case 1: Not accessed and Not modified.
                    // evict immediately
                    break;
                } else {
                    // this the not accessed but modified case
                    if (current_page_info->flags & THIRD_CHANCE) {
                        // if the THIRD CHANCE flag is set
                        // this means that the page has been passed
                        // twice by the clock head before now
                        // thus can be evicted
                        break;
                    } else {
                        // the page has only been passed once before
                        // set the THIRD CHANCE flag to indicate this
                        current_page_info->flags |= THIRD_CHANCE;
                    }
                }
            }
            current_page_info = current_page_info->prev;
        }
        break;
        default:
        // unknown policy
        // panic!
        assert(false);
    }

    void * start_address = get_vm_page_address(current_page_info->vm_page_index);

    printf(
        "Evicting vm page %d at physical frame %d starting at address %p and setting access to None\n",
        current_page_info->vm_page_index,
        current_page_info->physical_frame_index,
        start_address
    );

    mprotect(start_address, memory_page_size, PROT_NONE);

    return current_page_info;
}

// does the main memory management stuff
// returns the new access flags to set on the page
int access_memory(int vm_page_index, int offset, enum access_type type){
    struct PageInfo *info = find_page(vm_page_index);
    if (info == NULL) {
        // need to load the page from memory
        info = load_page(vm_page_index, offset, type);
    } else {
        int physical_address = info->physical_frame_index * memory_page_size + offset;
        int fault_type = -1;
        if (type == WRITE) {
            // write access to a already loaded page
            if(info->flags & MODIFIED) {
                // page was written to before
                info->flags |= ACCESSED;
                // so fault type is 4
                fault_type = 4;
            } else {
                // page was not written to before
                info->flags |= ACCESSED;
                info->flags |= MODIFIED;
                // fault type should be 2
                fault_type = 2;
            }
        } else {
            // this must be a read to a page that had the accessed bit cleared
            info->flags |= ACCESSED;
            // fault type is 3
            fault_type = 3;
        }

        // always clear the third chance flag if the memory is referenced at all
        info->flags &= (~THIRD_CHANCE);

        // log this access
        // if the page is already present no evictions are possible
        // so we should not 
        mm_logger(vm_page_index, fault_type, -1, 0, physical_address);
    }

    if (type == READ) {
        return PROT_READ;
    } else {
        return PROT_READ | PROT_WRITE;
    }
}

// finds the page info struct for the given vm page
// returns null if page is not found in phyiscal memory
struct PageInfo *find_page(int vm_page_index){
    struct PageInfo *info = current_page_info;

    if (info != NULL){
        do {
            if (info->vm_page_index == vm_page_index) {
                return info;
            }

            info = info->next;

        }while(info != current_page_info);
    }

    return NULL;
}

void * get_vm_page_address(int vm_page_index){
    return vm_start_address + (vm_page_index * memory_page_size);
}

size_t virtual_memory_size;
size_t memory_page_size;
size_t physical_frame_count;
size_t used_physical_frames;
struct PageInfo *current_page_info;
void *vm_start_address;
enum policy_type eviction_policy;












