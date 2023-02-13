#include "interface.h"
#include "vmm.h"

// Interface implementation
// Implement APIs here...

void mm_init(enum policy_type policy, void *vm, int vm_size, int num_frames, int page_size)
{

    //initalized variables
    vm_start_address = vm;
    virtual_memory_size = vm_size;
    physical_frame_count = num_frames;
    memory_page_size = page_size;
    eviction_policy = policy;

    // we start with no pages loaded in memory
    used_physical_frames = 0;
    current_page_info = NULL;

    // protect the entire region
    mprotect(vm, vm_size, PROT_NONE);

    struct sigaction sigaction_struct;
    sigaction_struct.sa_flags = SA_SIGINFO; // our handler is a three argument sigaction handler 
    sigemptyset(&sigaction_struct.sa_mask); // clear any masks.
    sigaction_struct.sa_sigaction = segv_handler; // set the handeler
    sigaction(SIGSEGV, &sigaction_struct, NULL); // apply the sigaction struct

}









