#include "usertraps.h"
#include "misc.h"

void main (int argc, char *argv[])
{
  int pid = 0;
  Printf("Testing fork...\n");

  pid = Fork();
  Printf("Fork returned pid = %d\n", pid);
  if( pid == 0)
  {
    Printf("Child process (%d) created\n", getpid());
    Exit();
  }
  else
  {
    Printf("Parent process (%d) created child process (%d)\n", getpid(), pid);
    Exit();
  }
}
