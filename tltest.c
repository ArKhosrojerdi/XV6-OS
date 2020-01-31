#include "types.h"
#include "user.h"

#define NCHILD 10

int main()
{
  int pid;
  ticketlockInit();

  pid = fork();
  int parent = pid;
  int i;
  for (i = 1; i < NCHILD; i++)
  {
    if (pid < 0)
    {
      printf(1, "fork failed\n");
      exit();
    }
    else if (pid > 0)
    {
      pid = fork();
    }
  }
  if (pid < 0)
  {
    printf(1, "fork failed\n");
    exit();
  }
  else if (pid == 0)
  {
    printf(1, "child adding to shared counter\n");
    ticketlockTest(); // extra line !!!
  }
  else
  {
    if (pid == parent)
    {
      sleep(1000);
    }
    for (i = 0; i < NCHILD; i++)
      wait();

    printf(1, "user program finished\n");
    printf(2, "ticket lock value: %d\n", ticketlockTest() - 1);
  }
  exit();

  return 0;
}