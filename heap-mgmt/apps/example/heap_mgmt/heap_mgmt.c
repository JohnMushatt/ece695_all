#include "usertraps.h"
#include "misc.h"

/**
 * Allocate 1 32-bit integer and free it (4 bytes)
 */
void test1(void)
{
  int val = 0;
  int memFreed = 0;
  int *ptr32 = (int *)malloc(sizeof(int));
  if(ptr32 == NULL)
  {
    Printf("\ntest1: malloc returned NULL\n\n");
    return;
  }
  *ptr32 = 0x100;
  if(*ptr32 != 0x100)
  {
    Printf("test1:[1/2] var32: %d != 0x100\n", *ptr32);
  }
  else
  {
    Printf("test1:[1/2] var32: %d == 0x100\n", *ptr32);
  }
  memFreed = mfree((void*)ptr32);
  if(memFreed != sizeof(int))
  {
    Printf("test1:[2/2] mfree returned: %d bytes freed != %d\n", memFreed, sizeof(int));
  }
  else
  {
    Printf("test1:[2/2] mfree returned: %d bytes freed == %d\n", memFreed, sizeof(int));
  }

  
  Printf("test1: Done!\n");
}

/**
Allocate 128 32-byte blocks - should return order 7 (128 bytes)
 */
void test2(void)
{
  int *arr[128];
  int *badMalloc = NULL;
  int i;
  int totalMem = 0;
  Printf("test2: [1/3] Allocating 128 32-byte blocks\n");
  for(i = 0; i < 128; i++)
  {
    arr[i] = malloc(32);
    if(arr[i] == NULL)
    {
      Printf("test2: %d: malloc(32) returned NULL\n", i);
      break;
    }
    else
    {
      totalMem += 32;
      //Printf("test2: %d: malloc(32) returned: %d\n", i, arr[i]);
    }
  }
  if(i != 128)
  {
    Printf("test2: [2/3] Only allocated %d of 128 blocks\n", i);
  }
  else
  {
    Printf("test2: [2/3] Allocated 128 blocks\n");
  }
  Printf("test2: [2/3] Total memory allocated: %d bytes, expected: %d bytes\n", totalMem, 128 * 32);
  Printf("test2: [3/3] Allocating 1 1-byte block after all blocks allocated\n");
  badMalloc = malloc(1);
  // Should return NULL
  if(badMalloc == NULL)
  {
    Printf("test2:[3/3] malloc(1) returned NULL\n");
  }
  else
  {
   Printf("test2:[3/3] malloc(1) returned: %d, which is not expected\n", badMalloc);
  }
  totalMem = 0;
  // free all memory
  for(i = 0; i < 128; i++)
  {
    totalMem += mfree((void *)arr[i]);
  }
  Printf("test2: [3/3] Total memory freed: %d bytes, expected: %d bytes\n", totalMem, 128 * 32);
  
}
void test3(void)
{
  int memFreed =0;
  void *big_alloc = (void *) malloc(4096);

  if(big_alloc == NULL)
  {
    Printf("test3: [1/1] malloc(4096) returned NULL\n");
  }
  else
  {
    Printf("test3: [1/1] malloc(4096) returned: %d\n", big_alloc);
  }
  {
    int *small_alloc = (int *) malloc(1);
    // Should return NULL
    if(small_alloc == NULL)
    {
      Printf("test3: [2/2] malloc(1) returned NULL\n");
    }
    else
    {
      Printf("test3: [2/2] malloc(1) returned: %d, which is not expected\n", small_alloc);
    }
  } 
  memFreed += mfree(big_alloc);
  Printf("test3: [2/2] Total memory freed: %d bytes, expected: %d bytes\n", memFreed, 4096);
}

void test4(void)
{

}
void main (int argc, char *argv[])
{
  

  test1();
  test2();
  test3();
  Printf("All tests done!\n");
}
