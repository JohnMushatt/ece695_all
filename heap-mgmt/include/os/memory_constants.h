#ifndef	_memory_constants_h_
#define	_memory_constants_h_

//------------------------------------------------
// #define's that you are given:
//------------------------------------------------

// We can read this address in I/O space to figure out how much memory
// is available on the system.
#define	DLX_MEMSIZE_ADDRESS	0xffff0000

// Return values for success and failure of functions
#define MEM_SUCCESS 1
#define MEM_FAIL -1

//--------------------------------------------------------
// Put your constant definitions related to memory here.
// Be sure to prepend any constant names with "MEM_" so 
// that the grader knows they are defined in this file.

//--------------------------------------------------------

// Constants for memory management
#define MEM_L1FIELD_FIRST_BITNUM 12  // 4KB pages = 2^12 bytes
#define MAX_VIRTUAL_ADDRESS 0x3FFFFF  // 4096KB = 4MB = 0x400000, so max address is 0x3FFFFF
#define MEM_MAX_PHYS_MEM 0x200000     // 2MB = 2*1024*1024 = 0x200000
#define MEM_PTE_READONLY 0x00000004   // Read-only bit in PTE
#define MEM_PTE_DIRTY 0x00000002      // Dirty bit in PTE  
#define MEM_PTE_VALID 0x00000001      // Valid bit in PTE


// Computed constans
#define MEM_PAGESIZE (1 << MEM_L1FIELD_FIRST_BITNUM)  // 4KB = 4096 bytes
#define MEM_ADDRESS_OFFSET_MASK (MEM_PAGESIZE - 1)    // Mask for page offset
#define MEM_MAX_VIRTUAL_PAGES (MAX_VIRTUAL_ADDRESS / MEM_PAGESIZE + 1)  // 1024 pages
#define MEM_MAX_PHYS_PAGES (MEM_MAX_PHYS_MEM / MEM_PAGESIZE)  // 512 pages
#define MEM_PTE_MASK (~(MEM_PTE_VALID | MEM_PTE_READONLY | MEM_PTE_DIRTY))  // Mask for physical address

// Process initialization constants
#define MEM_INIT_CODE_PAGES (4) // 4 pages for code (16KB)
#define MEM_INIT_USER_STACK_PAGES (1) // 1 page for user stack (4KB)
#define MEM_INIT_SYS_STACK_PAGES (1) // 1 page for system stack (4KB)


// Buddy Memory Allocation Constants
#define MEM_BUDDY_MIN_BLOCK_ORDER 5     // order 1 = 2^5 = 2 bytes
#define MEM_BUDDY_MIN_BLOCK_SIZE (1 << MEM_BUDDY_MIN_BLOCK_ORDER)
#define MEM_BUDDY_MAX_BLOCK_ORDER 12   // order 6 = 2 ^ 12 = 4096 bytes (one page)
#define MEM_BUDDY_NUM_ORDERS (MEM_BUDDY_MAX_BLOCK_ORDER - MEM_BUDDY_MIN_BLOCK_ORDER + 1)

// Heap allocation constants
#define MEM_HEAP_START 4 * MEM_PAGESIZE  // Virtual address where heap starts (after code/data)
#define MEM_HEAP_SIZE MEM_PAGESIZE       // Heap is 1 page = 4KB
#define MAX_BUDDY_NODES 256

#endif	// _memory_constants_h_
