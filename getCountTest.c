#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char *argv[])
{
  int scno = atoi(argv[1]);
  // fork();
  // wait();
  // fork();
  // wait();
  getpid();
  getpid();
  getpid();
  getpid();
  getpid();
  getpid();
  // printf(1, "hi\n");
  printf(1, "hi\n");
  getCount(scno);
  exit();
  return 1;
}