#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char *argv[])
{
  int sc = atoi(argv[1]);
  fork();
  wait();
  // fork();
  // wait();
  // getpid();
  // getpid();
  // getpid();
  // getpid();
  // getpid();
  // getpid();
  getCount(sc);
  exit();
  return 1;
}