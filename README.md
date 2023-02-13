# Virtual_Memory_Manager-CPMSC473-2022

![Test Status](https://github.com/PSU473/p3-2022-manager-Austinguo63/actions/workflows/tests.yml/badge.svg)

## Author
Jinyu Liu (JZL6359@psu.edu)

Hongyu Guo (HBG5147@psu.edu)

## Introduction
This project focuses on implementing virtual memory paging 
controls under limited physical memory. With two 
page replacement policies when no physical frame is free and 
one need to evicted to make space.
 


## Theory of Operation

### PageInfo header
This header holds metadata for each used physical frame.
contains a flags field that holds the accessed(or referenced) and 
modified flags. As well as the a third flag for use with the
third chance replacement policy.

- `flags` : bit 0 is accessed, bit 1 is modified, bit 2 is third_chance.
- `vm_page_index` : location in virtual memory.
- `physical_frame_index`: location in physical memory.



### access_type
This function will only work on a x86/64 computer. Using hints from 
project 3 description, we find our `error_code` from  `REG_ERR`. By look 
into second bit of ERR register. 1 is WRITE and 0 is READ.

### segv_handler
This handler will first get location of error. Use `access_type` function to get access type.
 And check if error memory location is inside 
the vm space that assigned. If location not inside assigned location, it will abort the program since it is caused by our code and not by memory
accesses. The handler would then call `access_memory` which handles the
access and returns the new protection flags for the page. Which is then
applied using `mprotect`.

### load_page
load_page will have two scenarios:

First case: 
When we have space in physical memory,
allocate new `PageInfo` by using `malloc` and insert this page in to circular linked list.

Second case:
When we do not have space in physical memory, use `evict_page`
function to evict a page.
And if this evicted page has been modified, set `write_back` to 1 to show that it needs to be written back.
The virtual memory location in the page info header of the evicted
is overwritten with the location of the loaded page and the flags are set
accordingly.

Use `mm_logger` to log this access and return a pointer to the `PageInfo` 
of the loaded page. 

### evict_page 
For FIFO replacement policy:

We only need to move it to previous location. 
Since it is a round buffer. And load will always add page to the end, move
previous will get us to the location of first page in this buffer. 

For third_chance policy:

We move along the circular list, for each page:
If page has been accessed, we need clear both accessed flag
and third_chance flag. We also need to set permissions for the
page to `PROT_NONE` to make sure the next access to this page
triggers the handler to set the accessed flag again.

If page has not been accessed and not been modified, evict immediately.

In case of page been accessed but been modified.  If third_chance flag is 
not set,
that mean it only been passed once before, and we set the third_chance flag
If third_chance flag is set, that mean we have already passed this page twice before, thus we can evict it.
For a page that is accessed and modified.
First time set to (access modified third_chance) 0 1 0 , second 0 1 1, third time will evict it.

Set the evict page to `PORT_NONE` using mprotect, return its `PageInfo`.


### access_memory function
This function first use `find_page` to find if this page is already loaded.
If we have this page, then set is flags according to access type.
Logging the change in `mm_logger`.
If the page is not present in physical memory, use `load_page` to load
load it into physical memory, possibly evicting a page in the process.

read access would set the page to read only.
write access would set the page to read and write.

### find_page function
find_page function search through circular linked-list to see if we have this page in physical memory or not or not.
It will return a pointer to the `PageInfo` of the page if it is found,
returns NULL otherwise.
















