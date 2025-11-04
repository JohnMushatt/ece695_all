#include "usertraps.h"
#include "misc.h"

/**
 * Allocate 1 32-bit integer and free it (4 bytes)
 */
void test1(void)
{
  int num_test = 3;
  char *ptr = (char*) malloc(8192);
  if(ptr == NULL)
  {
    Printf("test1: [1/%d] malloc(4096) returned NULL\n", num_test);
  }
  else
  {
    Printf("test1: [1/%d] malloc(4096) returned: %d\n", num_test, ptr);
  }
  Printf("test1: Writing to first heap page\n");
  ptr[0] = 'A';
  Printf("test1: Written to first heap page\n");
  Printf("test1: Writing to second heap page\n");
  ptr[4096] = 'B';
  Printf("test1: Written to second heap page\n");
  Printf("test1: Attempting to write to out of bounds\n");

  ptr[8192] = 'C';
}

void test2(void)
{
  char *ptr = (char*) malloc(65536);

  Printf("test2: Attempting to allocate 64KB of memory\n");
  if(ptr == NULL)
  {
    Printf("test2: [1/1] malloc(65536) returned NULL\n");
  }
  else
  {
    Printf("test2: [1/1] malloc(65536) returned: %d\n", ptr);
  }
  Printf("test2: Writing to first 4096 bytes of the allocated memory\n");
  ptr[0] = 'A';
  Printf("test2: Written to first 4096 bytes of the allocated memory\n");
  Printf("test2: Writing to second 4096 bytes of the allocated memory\n");
  ptr[4096] = 'B';
  Printf("test2: Written to last 4096 bytes of the allocated memory\n");
  ptr[65344] = 'C';
  Printf("test2: Written to last 65344 bytes of the allocated memory\n");
  Printf("test2: Attempting to write to out of bounds\n");
  ptr[65536] = 'D';
  Printf("test2: Written to out of bounds\n");
}

void main (int argc, char *argv[])
{
  

 // test1();
  test2();
  Printf("All tests done!\n");
}
