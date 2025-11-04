//
//	memory.c
//
//	Routines for dealing with memory management.

//static char rcsid[] = "$Id: memory.c,v 1.1 2000/09/20 01:50:19 elm Exp elm $";

#include "ostraps.h"
#include "dlxos.h"
#include "process.h"
#include "memory.h"
#include "queue.h"

// num_pages = size_of_memory / size_of_one_page
static uint32 freemap[MEM_MAX_PHYS_PAGES / 32];
static uint32 pagestart;
static int nfreepages;
static int freemapmax;

// Typedefs



static BuddyNode *allocate_buddy_node(PCB *pcb)
{
  BuddyNode *node = NULL;
  int i = 0;
  // If not null then there are free nodes. Check free list first before checking pool.
  /*
  if(pcb->buddy_free_list != NULL)
  {
    node = &pcb->buddy_free_list;
    //pcb->buddy_node_pool[i].in_use = 1;
    node->order = 0;
    node->is_allocated = 0;
    node->left_child = NULL;
    node->right_child = NULL;
    node->parent = NULL;
    node->addr = 0;
    dbprintf('m', "allocate_buddy_node: Allocated node %p at index [%d/%d]\n", node, i, MAX_BUDDY_NODES-1);
    return node;
  }
  */
  /*
  // If the pool is full and free list is empty return NULL
  if(pcb->buddy_node_pool_index >= MAX_BUDDY_NODES)
  {
    printf("allocate_buddy_node: Buddy node pool is full\n");
    return NULL;
  }
    */
  // If the pool is not full, allocate a new node from the pool
  //node = &pcb->buddy_node_pool[pcb->buddy_node_pool_index++];
  
  for(i = 0; i < MAX_BUDDY_NODES; i++)
  {
    if(pcb->buddy_node_pool[i].in_use == 0)
    {
      node = &pcb->buddy_node_pool[i];
      pcb->buddy_node_pool[i].in_use = 1;
      node->order = 0;
      node->is_allocated = 0;
      node->left_child = NULL;
      node->right_child = NULL;
      node->parent = NULL;
      node->addr = 0;
      dbprintf('m', "allocate_buddy_node: Allocated node %p at index [%d/%d]\n", node, i, MAX_BUDDY_NODES-1);
      return node;
    }
  }
  dbprintf('m', "allocate_buddy_node: No free nodes found\n");
  //dbprintf('m', "allocate_buddy_node: Allocated node %p at index [%d/%d]\n", node, pcb->buddy_node_pool_index, MAX_BUDDY_NODES-1);
  return node;
}
static void free_buddy_node(PCB *pcb, BuddyNode *node)
{
  if(node == NULL)
  {
    dbprintf('m', "free_buddy_node: Node is NULL\n");
    return;
  }
  if( node < &pcb->buddy_node_pool[0] || node > &pcb->buddy_node_pool[MAX_BUDDY_NODES-1])
  {
    dbprintf('m', "free_buddy_node: Node is not in the buddy node pool\n");
    return;
  }

  node->in_use = 0;
  dbprintf('m', "free_buddy_node: Freed node %p\n", node);
}

void BuddyAllocatorInit(PCB *pcb)
{
  int i = 0;

  dbprintf('m', "BuddyAllocatorInit: Initializing buddy allocator with %d nodes, max heap size = 0x%04x bytes\n", MAX_BUDDY_NODES, MEM_HEAP_SIZE);
  for(i = 0; i < MAX_BUDDY_NODES; i++)
  {
    pcb->buddy_node_pool[i].in_use = 0;
  }
  dbprintf('m', "BuddyAllocatorInit: Set all nodes to not in use\n");
  pcb->buddy_node_pool_index = 0;
  //pcb->buddy_free_list = NULL;
  // /dbprintf('m', "BuddyAllocatorInit: Initialized buddy allocator, free list = %p\n", pcb->buddy_free_list);
  if( (pcb->pagetable[4] & MEM_PTE_VALID) == 0)
  {
    dbprintf('m', "BuddyAllocatorInit: Page 4 is not valid, killing process %d\n", GetPidFromAddress(pcb));
    ProcessKill();
    return;
  }
  // Allocate a buddy node for the root

  pcb->buddy_root =  allocate_buddy_node(pcb);
  if(pcb->buddy_root == NULL)
  {
    dbprintf('m', "BuddyAllocatorInit: Failed to allocate buddy root\n");
    ProcessKill();
    return;
  }
  pcb->buddy_root->order = MEM_BUDDY_MAX_BLOCK_ORDER;
  pcb->buddy_root->is_allocated = 0;
  pcb->buddy_root->left_child = NULL;
  pcb->buddy_root->right_child = NULL;
  pcb->buddy_root->parent = NULL;
  pcb->buddy_root->addr = 0;
  dbprintf('m', "BuddyAllocatorInit: Initialized buddy allocator, root node order = %d\n", pcb->buddy_root->order);

  
}

static BuddyNode *find_best_fit(BuddyNode *node, int order)
{
  if(node == NULL)
  {
    //dbprintf('m', "find_best_fit: Node is NULL\n");
    return NULL;
  }
 // dbprintf('m', "find_best_fit: Node address: %p, order: %d, is_allocated: %d\n", node, node->order, node->is_allocated);
  if(node->is_allocated)
  {
    //dbprintf('m', "find_best_fit: Node is allocated\n");
    return NULL;
  }
  if(node->order < order)
  {
    //dbprintf('m', "find_best_fit: Node order is less than requested order\n");
    return NULL;
  }

  if(node->left_child != NULL || node->right_child != NULL)
  {
    BuddyNode *result = NULL;
    
    if(node->left_child != NULL)
    {
      result = find_best_fit(node->left_child, order);
      if(result != NULL)
      {
        return result;
      }
    }

    if(node->right_child != NULL)
    {
      result = find_best_fit(node->right_child, order);
      if(result != NULL)
      {
        return result;
      }
    }

    // Children exist but no suitable block found - return NULL
    return NULL;
  }

  if(node->order == order)
  {
    //dbprintf('m', "find_best_fit: Node order is equal to requested order\n");
    return node;
  }
  // If we get here, no suitable block found
  //dbprintf('m', "find_best_fit: No suitable block found, returning node %p\n", node);
  return node;
}
/*
  Split a buddy node into two smaller nodes.
  The node is the parent of the two new nodes.
  The two new nodes are created by splitting the parent node.
  The left child is the new node with the same order as the parent.
  The right child is the new node with the same order as the parent.
*/
static void split_buddy_node(PCB *pcb, BuddyNode *node)
{

  uint32 childOrder = node->order - 1;
  uint32 childSize = 1 << childOrder;
  uint32 halfChildSize = 1 << (node->order - 1);

  //printf("split_buddy_node: Splitting node: order = %d, size = %d, halfChildSize = %d\n", 
  //  node->order, childSize, halfChildSize);
  // If the node is already at the minimum order, do nothing
  if(node->order == MEM_BUDDY_MIN_BLOCK_ORDER)
  {
    return;
  }
  // Allocate a new buddy node for the left child
  node->left_child = allocate_buddy_node(pcb);
  if(node->left_child == NULL)
  {
    dbprintf('m', "split_buddy_node: Failed to allocate left child\n");
    return;
  }
  node->left_child->order = childOrder;
  node->left_child->is_allocated = 0;

  node->left_child->addr = node->addr;
  node->left_child->parent = node;

  node->left_child->left_child = NULL;
  node->left_child->right_child = NULL;
  printf("Created a left child node (order = %d, addr = %d, size = %d) of parent (order = %d, addr = %d, size = %d)\n",
    childOrder-5, node->left_child->addr, childSize, node->order-5, node->addr, 1 << node->order);
  // Allocate a new buddy node for the right child

  node->right_child = allocate_buddy_node(pcb);
  if(node->right_child == NULL)
  {
    dbprintf('m', "split_buddy_node: Failed to allocate right child\n");
    return;
  }
  node->right_child->order = childOrder;
  node->right_child->is_allocated = 0;
  node->right_child->addr = node->addr + halfChildSize;
  node->right_child->parent = node;
  node->right_child->left_child = NULL;
  node->right_child->right_child = NULL;
  printf("Created a right child node (order = %d, addr = %d, size = %d) of parent (order = %d, addr = %d, size = %d)\n",
    childOrder-5, node->right_child->addr, childSize, node->order-5, node->addr, 1 << node->order);
}
void *malloc(PCB *pcb, uint32 size)
{
  uint32 heap_vaddr = NULL;
  uint32 blockSize = MEM_BUDDY_MIN_BLOCK_SIZE;
  int order = MEM_BUDDY_MIN_BLOCK_ORDER;
  BuddyNode *target = NULL;
  uint32 roundedSize = size;


  if(size <= 0 || size > MEM_HEAP_SIZE)
  {
    dbprintf('m', "malloc: invalid size: %d\n", size);
    return NULL;
  }
  
  if(size %4 != 0)
  {
    roundedSize = ((size / 4) + 1) * 4;
  }
  if(pcb->buddy_root == NULL)
  {
    dbprintf('m', "malloc: buddy root is not initialized\n");
    BuddyAllocatorInit(pcb);

  }
  // Find the smallest block that is greater than or equal to the requested size
  while(blockSize < size && order < MEM_BUDDY_MAX_BLOCK_ORDER)
  {
    order++;
    blockSize  = 1 << order;
  }
  dbprintf('m', "malloc: block size %d fits requested size %d, order = %d\n", blockSize, size, order);
  if( blockSize > MEM_HEAP_SIZE)
  {
    dbprintf('m', "malloc: requested size %d is greater than heap size %d\n", size, MEM_HEAP_SIZE);
    ProcessKill();
    return NULL;
  }

  dbprintf('m', "malloc: finding best fit for order %d for buddy root %p\n", order, pcb->buddy_root);
  target = find_best_fit(pcb->buddy_root, order);
  dbprintf('m', "malloc: found best fit: target = %p, is NULL = %d\n", target, target== NULL);
  if( !target)
  {
    dbprintf('m', "malloc: no suitable block found\n");
    return NULL;
  }
  while(target->order > order)
  {
    //printf("malloc: splitting buddy node: order = %d, addr = %d, size = %d\n", target->order, target->addr, 1 << target->order);
    split_buddy_node(pcb, target);
    if(target->left_child == NULL)
    {
      dbprintf('m', "malloc: Failed to split buddy node: order = %d, addr = %d, size = %d\n", target->order, target->addr, 1 << target->order);
      return NULL;
    }
    target = target->left_child;
  }

  target->is_allocated = 1;
  target->user_size = size;
  heap_vaddr = MEM_HEAP_START + target->addr;
  printf("Allocated the block: order = %d, addr = %d, requested mem size = %d, block size = %d\n",
     target->order-5, target->addr, size, blockSize);
  return (void *)heap_vaddr;
}

int mfree(PCB *pcb, void* ptr)
{
  int bytesFreed = 0;
  uint32 heap_vaddr = (uint32)ptr;
  uint32 addr_offset = heap_vaddr - MEM_HEAP_START;
  BuddyNode *target = NULL;
  BuddyNode *current = NULL;
  uint32 phyaddr = 0;

  if(ptr == NULL)
  {
    dbprintf('m', "mfree: Invalid pointer: %p\n", ptr);
    return MEM_FAIL;
  }
  if(pcb->buddy_root == NULL || addr_offset < 0 || addr_offset >= MEM_HEAP_SIZE)
  {
    dbprintf('m', "mfree: Invalid pointer: %p\n", ptr);
    return MEM_FAIL;
  }
  target = pcb->buddy_root;
  if(target == NULL)
  {
    dbprintf('m', "mfree: Node not found\n");
  }

  while(target != NULL)
  {
    // If the target node is the one we are looking for and it is allocated, we have found the block
    if(target->addr == addr_offset && target->is_allocated)
    {
      break;
    }
    
    if(target->left_child == NULL && target->right_child == NULL)
    {
      break;
    }

    if(target->left_child != NULL && addr_offset >= target->left_child->addr && 
      addr_offset < target->left_child->addr + (1 << target->left_child->order))
    {
      target = target->left_child;
    }
    else if(target->right_child != NULL && addr_offset >= target->right_child->addr && 
            addr_offset < target->right_child->addr + (1 << target->right_child->order))
    {
      target = target->right_child;
    }
    else
    {
      dbprintf('m', "mfree: Block address not found in tree\n");
      return MEM_FAIL;
    }
  }
  if(target->addr != addr_offset || !target->is_allocated)
  {
    dbprintf('m', "mfree: Block address not found in tree with addr_offset = %d and target->addr = %d\n", addr_offset, target->addr);
    return MEM_FAIL;
  }


  phyaddr = MemoryTranslateUserToSystem(pcb, ptr);
  if(phyaddr == 0)
  {
    dbprintf('m', "mfree: Failed to translate user address to system address\n");
    return MEM_FAIL;
  }
  bytesFreed = target->user_size;
  target->is_allocated = 0;
  printf("Freed the block: order %d, addr = %d, size = %d\n",
     target->order-5, target->addr, bytesFreed);
  
  // Handle coalescing
  current = target;
  while( current->parent != NULL)
  {
    BuddyNode *parent = current->parent;
    BuddyNode *sibling = NULL;
    if(parent->left_child == current)
    {
      sibling = parent->right_child;
    }
    else
    {
      sibling = parent->left_child;
    }

    // If the sibling is not allocated, we can coalesce
    if(sibling != NULL && !sibling->is_allocated 
      && sibling->left_child == NULL 
      && sibling->right_child == NULL)
    {
      printf("Coalesced buddy nodes (order = %d, addr = %d, size = %d) & (order = %d, addr = %d, size = %d)\n",
        current->order-5, current->addr, 1 << current->order,
        sibling->order-5, sibling->addr, 1 << sibling->order);
      printf("into the parent node (order = %d, addr = %d, size = %d)\n",
              parent->order-5, parent->addr, 1 << parent->order);   

      free_buddy_node(pcb, current);
      free_buddy_node(pcb, sibling);
      parent->is_allocated = 0;
      parent->left_child = NULL;
      parent->right_child = NULL;

      current = parent;
    
    }
    else
    {
      break;
    }
  }
  return bytesFreed;
}
//----------------------------------------------------------------------
//
//	This silliness is required because the compiler believes that
//	it can invert a number by subtracting it from zero and subtracting
//	an additional 1.  This works unless you try to negate 0x80000000,
//	which causes an overflow when subtracted from 0.  Simply
//	trying to do an XOR with 0xffffffff results in the same code
//	being emitted.
//
//----------------------------------------------------------------------
static int negativeone = 0xFFFFFFFF;
static inline uint32 invert (uint32 n) {
  return (n ^ negativeone);
}

//----------------------------------------------------------------------
//
//	MemoryGetSize
//
//	Return the total size of memory in the simulator.  This is
//	available by reading a special location.
//
//----------------------------------------------------------------------
int MemoryGetSize() {
  return (*((int *)DLX_MEMSIZE_ADDRESS));
}

static inline void MemorySetFreemap(int p, int bit) {
  uint32 wd = p / 32;
  uint32 bitnum = p % 32;
  //dbprintf('m', "MemorySetFreemap: Setting freemap entry %d to %d\n", p, bit);
  freemap[wd] = (freemap[wd] & invert(1 << bitnum)) | (bit << bitnum);
  //dbprintf('m', "MemorySetFreemap: Set freemap entry %d to 0x%x.\n", wd, freemap[wd]);
}

//----------------------------------------------------------------------
//
//	MemoryModuleInit
//
//	Initialize the memory module of the operating system.
//      Basically just need to setup the freemap for pages, and mark
//      the ones in use by the operating system as "VALID", and mark
//      all the rest as not in use.
//
//----------------------------------------------------------------------
void MemoryModuleInit() {
  int i;
  int maxPage = MemoryGetSize() / MEM_PAGESIZE;
  int curpage;

  // Check for max page being greater than max physical pages
  if( maxPage > MEM_MAX_PHYS_PAGES)
  {
    maxPage = MEM_MAX_PHYS_PAGES;
    dbprintf('m', "Max page is greater than max physical pages, setting max page to %d\n", maxPage);
  }
  pagestart = (lastosaddress + MEM_PAGESIZE - 4) / MEM_PAGESIZE;
  freemapmax = (maxPage + 31) /32;


  dbprintf('m', "maxPage: %d\n", maxPage);
  for (i = 0; i < freemapmax; i++) {
    //dbprintf('m', "freemap[%d]: %x\n", i, freemap[i]);
   // MemorySetFreemap(i, 0);
    freemap[i] = 0;
  }
  nfreepages = 0;
  for(curpage = pagestart; curpage < maxPage; curpage++) {
    MemorySetFreemap(curpage, 1);
    nfreepages += 1;
  }
  //dbprintf('m', "pagestart: %d, nfreepages: %d\n", pagestart, nfreepages);

  dbprintf('m', "Leaving MemoryModuleInit\n");
  dbprintf('m', "Initialized %d free pages, freemapmax: %d, pagestart: %d\n", nfreepages, freemapmax, pagestart);
}


//----------------------------------------------------------------------
//
// MemoryTranslateUserToSystem
//
//	Translate a user address (in the process referenced by pcb)
//	into an OS (physical) address.  Return the physical address.
//
//----------------------------------------------------------------------
uint32 MemoryTranslateUserToSystem (PCB *pcb, uint32 addr) {
  /**
   * 1. Check if address is invalid
   * 2. Grab the virtual page number by shifting out offset
   * 3. Grab the offset by masking out the virtual page number
   * 4. Get the PTE for the page
   * 5. Check if the PTE is valid
   * 6. Get the physical address of the page
   * 7. Return the physical address
   */
  uint32 vPageNum = 0;
  uint32 offset = 0;
  uint32 pte = 0;
  uint32 phyAddr = 0;
  // Check if address is invalid  
  if( addr > MAX_VIRTUAL_ADDRESS)
  {
    dbprintf('m', "Invalid address: %04x\n", addr);
    return 0;
  }
  // Grab the virtual page number by shifting out offset
  vPageNum = addr >> MEM_L1FIELD_FIRST_BITNUM;
  // Grab the offset by masking out the virtual page number
  offset = addr & MEM_ADDRESS_OFFSET_MASK;

  pte = pcb->pagetable[vPageNum];

  if( (pte & MEM_PTE_VALID) == 0)
  {
    dbprintf('m', "Invalid PTE for virtual page %d\n", vPageNum);
    return 0;
  }

  phyAddr = (pte & MEM_PTE_MASK) | offset;
  //dbprintf('m', "Physical address: with OR offset vs + offset: 0x%04x vs 0x%04x\n", phyAddr | offset, phyAddr + offset);

  return phyAddr;
}


//----------------------------------------------------------------------
//
//	MemoryMoveBetweenSpaces
//
//	Copy data between user and system spaces.  This is done page by
//	page by:
//	* Translating the user address into system space.
//	* Copying all of the data in that page
//	* Repeating until all of the data is copied.
//	A positive direction means the copy goes from system to user
//	space; negative direction means the copy goes from user to system
//	space.
//
//	This routine returns the number of bytes copied.  Note that this
//	may be less than the number requested if there were unmapped pages
//	in the user range.  If this happens, the copy stops at the
//	first unmapped address.
//
//----------------------------------------------------------------------
int MemoryMoveBetweenSpaces (PCB *pcb, unsigned char *system, unsigned char *user, int n, int dir) {
  unsigned char *curUser;         // Holds current physical address representing user-space virtual address
  int		bytesCopied = 0;  // Running counter
  int		bytesToCopy;      // Used to compute number of bytes left in page to be copied

  while (n > 0) {
    // Translate current user page to system address.  If this fails, return
    // the number of bytes copied so far.
    curUser = (unsigned char *)MemoryTranslateUserToSystem (pcb, (uint32)user);

    // If we could not translate address, exit now
    if (curUser == (unsigned char *)0) break;

    // Calculate the number of bytes to copy this time.  If we have more bytes
    // to copy than there are left in the current page, we'll have to just copy to the
    // end of the page and then go through the loop again with the next page.
    // In other words, "bytesToCopy" is the minimum of the bytes left on this page 
    // and the total number of bytes left to copy ("n").

    // First, compute number of bytes left in this page.  This is just
    // the total size of a page minus the current offset part of the physical
    // address.  MEM_PAGESIZE should be the size (in bytes) of 1 page of memory.
    // MEM_ADDRESS_OFFSET_MASK should be the bit mask required to get just the
    // "offset" portion of an address.
    bytesToCopy = MEM_PAGESIZE - ((uint32)curUser & MEM_ADDRESS_OFFSET_MASK);
    
    // Now find minimum of bytes in this page vs. total bytes left to copy
    if (bytesToCopy > n) {
      bytesToCopy = n;
    }

    // Perform the copy.
    if (dir >= 0) {
      bcopy (system, curUser, bytesToCopy);
    } else {
      bcopy (curUser, system, bytesToCopy);
    }

    // Keep track of bytes copied and adjust addresses appropriately.
    n -= bytesToCopy;           // Total number of bytes left to copy
    bytesCopied += bytesToCopy; // Total number of bytes copied thus far
    system += bytesToCopy;      // Current address in system space to copy next bytes from/into
    user += bytesToCopy;        // Current virtual address in user space to copy next bytes from/into
  }
  return (bytesCopied);
}

//----------------------------------------------------------------------
//
//	These two routines copy data between user and system spaces.
//	They call a common routine to do the copying; the only difference
//	between the calls is the actual call to do the copying.  Everything
//	else is identical.
//
//----------------------------------------------------------------------
int MemoryCopySystemToUser (PCB *pcb, unsigned char *from,unsigned char *to, int n) {
  return (MemoryMoveBetweenSpaces (pcb, from, to, n, 1));
}

int MemoryCopyUserToSystem (PCB *pcb, unsigned char *from,unsigned char *to, int n) {
  return (MemoryMoveBetweenSpaces (pcb, to, from, n, -1));
}

//---------------------------------------------------------------------
// MemoryPageFaultHandler is called in traps.c whenever a page fault 
// (better known as a "seg fault" occurs.  If the address that was
// being accessed is on the stack, we need to allocate a new page 
// for the stack.  If it is not on the stack, then this is a legitimate
// seg fault and we should kill the process.  Returns MEM_SUCCESS
// on success, and kills the current process on failure.  Note that
// fault_address is the beginning of the page of the virtual address that 
// caused the page fault, i.e. it is the vaddr with the offset zero-ed
// out.
//
// Note: The existing code is incomplete and only for reference. 
// Feel free to edit.
//---------------------------------------------------------------------
int MemoryPageFaultHandler(PCB *pcb) {
  uint32 *tempStackFrame = pcb->currentSavedFrame;
  uint32 faultAddr    = tempStackFrame[PROCESS_STACK_FAULT];
  uint32 userStackPtr = pcb->currentSavedFrame[PROCESS_STACK_USER_STACKPOINTER];
  uint32 vPageNum     = faultAddr >> MEM_L1FIELD_FIRST_BITNUM;
  uint32 usp_vPageNum = (userStackPtr - 8) >> MEM_L1FIELD_FIRST_BITNUM;
  int     physPage = 0;

  dbprintf('m', "MemoryPageFaultHandler: Fault address: 0x%04x, vPageNum: %d, usp_vPageNum: %d\n", 
           faultAddr, vPageNum, usp_vPageNum);

  // Check if we've reached maximum memory limit
  if(pcb->npages >= MEM_MAX_VIRTUAL_PAGES)
  {
    dbprintf('m', "MemoryPageFaultHandler: Max memory reached, killing process %d\n", GetPidFromAddress(pcb));
    ProcessKill();
    return MEM_FAIL;
  }

  // Check if this is a legitimate user stack growth request
  // The fault page must be >= (userStackPtr - 8) page number
  // This allows for the 8 bytes (2 words) that the callee writes before updating the stack pointer
  if (vPageNum >= usp_vPageNum) {
    // Check if PTE already marked as valid (shouldn't happen for a fault)
    if(pcb->pagetable[vPageNum] & MEM_PTE_VALID)
    {
      dbprintf('m', "MemoryPageFaultHandler: Page %d already allocated but caused fault, killing process %d\n", 
               vPageNum, GetPidFromAddress(pcb));
      ProcessKill();
      return MEM_FAIL;
    }
    
    // Allocate a new page for stack growth
    physPage = MemoryAllocPage();
    if(physPage == MEM_FAIL)
    {
      dbprintf('m', "MemoryPageFaultHandler: Failed to allocate page for stack growth, killing process %d\n", 
               GetPidFromAddress(pcb));
      ProcessKill();
      return MEM_FAIL;
    }

    // Set up the PTE for the new stack page
    pcb->pagetable[vPageNum] = MemorySetupPte(physPage);
    
    // Zero out the new page
    bzero((char *)(physPage << MEM_L1FIELD_FIRST_BITNUM), MEM_PAGESIZE);
    
    // Update page count
    pcb->npages++;
    
    // Copy the updated PTE to physical page table
    {
      uint32 *physPageTable = (uint32 *)pcb->pt_phys_base;
      physPageTable[vPageNum] = pcb->pagetable[vPageNum];
      
      dbprintf('m', "MemoryPageFaultHandler: Allocated new stack page, vPageNum=%d, physPage=%d, npages=%d\n", 
               vPageNum, physPage, pcb->npages);
    }
    return MEM_SUCCESS;
  }

  // If we get here, the fault is NOT within the allowed stack growth range - it's a segfault
  dbprintf('m', "MemoryPageFaultHandler: SEGFAULT - Invalid page fault (vPageNum: %d, stack allowed from page: %d), killing process %d\n", 
           vPageNum, usp_vPageNum, GetPidFromAddress(pcb));
  ProcessKill();
  return MEM_FAIL;
}


//---------------------------------------------------------------------
// You may need to implement the following functions and access them from process.c
// Feel free to edit/remove them
//---------------------------------------------------------------------

int MemoryAllocPage(void) {
  static int mapnum = 0;
  int pagenum = 0;
  int bitnum = 0;
  uint32 vaddr;

  if(nfreepages == 0)
  {
    dbprintf('m', "No free pages available, nfreepages = %d\n", nfreepages);
    return MEM_FAIL;
  }


  //dbprintf('m', "Allocating memory, starting with map %d\n", mapnum);
  // Will run until we find a free page
  while(freemap[mapnum] == 0)
  {
    mapnum++;
    if(mapnum >= freemapmax)
    {
      mapnum = 0;
    }
  }
  //At this point we have found a free page
  vaddr = freemap[mapnum];

  // Find the first free bit in this word
  for(bitnum = 0; bitnum < 32; bitnum++) {
    if((vaddr & (1 << bitnum))) {
      break;
    }
  }

  // Multiply the map number by 32 and add the bit number to get the physical page number
  pagenum = (mapnum * 32) + bitnum;

  // Clear bit in freemap
  MemorySetFreemap(pagenum, 0);
  nfreepages -= 1;
  //dbprintf('m', "Allocated physical page %d, from map %d, bit %d, map=0x%x.\n",
    //        pagenum, mapnum, bitnum, vaddr);
  return pagenum;
}

uint32 MemorySetupPte (uint32 page) {


  // Setup PTE for the page
  // Set the PTE to valid
  // Set the PTE to the physical page number
  // Return the PTE

  // Get the PTE for the page
  //uint32 pte = (page << MEM_L1FIELD_FIRST_BITNUM) | MEM_PTE_VALID;
  uint32 pte = (page * MEM_PAGESIZE) | MEM_PTE_VALID;
  //dbprintf('m', "Setup PTE for page %d, PTE=0x%x.\n", page, pte);
  return pte;
}


void MemoryFreePage(uint32 page) {
  uint32 mapWord =0;
  if( page>= pagestart && page < MEM_MAX_PHYS_PAGES)
  {
    MemorySetFreemap(page, 1);
    nfreepages += 1;
    mapWord = page / 32;
    dbprintf('m', "Freed physical page %d,  map [%d] = 0x%x.\n", page, mapWord, freemap[mapWord]);
  }
  else
  {
    dbprintf('m', "Tried to free page %d, but it is not in the valid range.\n", page);
  }
}

