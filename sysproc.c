#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

int sysc[1000];
int syscCounter = 0;

int sys_fork(void)
{
  sysc[syscCounter] = 1;
  syscCounter = syscCounter + 1;
  return fork();
}

int sys_exit(void)
{
  sysc[syscCounter] = 2;
  syscCounter = syscCounter + 1;
  exit();
  return 0; // not reached
}

int sys_wait(void)
{
  sysc[syscCounter] = 3;
  syscCounter = syscCounter + 1;
  return wait();
}

int sys_kill(void)
{
  int pid;

  if (argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int sys_getpid(void)
{
  return myproc()->pid;
}

int sys_sbrk(void)
{
  int addr;
  int n;

  if (argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

int sys_sleep(void)
{
  int n;
  uint ticks0;

  if (argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (myproc()->killed)
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

void sys_getChildren(void)
{
  int pid;
  argint(0, &pid);
  // if (myproc()->parent->pid == pid)
  // {
  //   cprintf("%d -> ", myproc()->parent->pid);
  //   cprintf("%d\n", myproc()->pid);
  // }
  iterateProcesses(pid);
  if (pid == myproc()->pid)
    printPIDString();
  exit();
}

int sys_getCount(void)
{
  int scno;
  argint(0, &scno);
  return getCount(scno);
}

int sys_changePriority(void)
{
  int pr;
  if (argint(0, &pr) < 0)
    return -1;

  return changePriority(pr);
  return 1;
}

int sys_cps(void)
{
  return cps();
}
