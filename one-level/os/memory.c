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
    dbprintf('m', "No free pages available.\n");
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

