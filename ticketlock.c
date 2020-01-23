

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "ticketlock.h"

int ticket = -1;

void initTicketlock(struct ticketlock *lk, char *name)
{
  lk->name = name;
  lk->locked = fetch_and_add(&ticket, 1);
  lk->cpu = 0;
}

void acquireTicketlock(struct ticketlock *lk)
{
  pushcli();
  if (holding(lk))
    panic("acquire");

  while (lk->locked != myproc()->ticket_no)
    ;

  __sync_synchronize();
  lk->cpu = mycpu();
  getcallerpcs(&lk, lk->pcs);
}

void releaseTicketlock(struct ticketlock *lk)
{
  if (!holding(lk))
    panic("release");

  lk->pcs[0] = 0;
  lk->cpu = 0;

  __sync_synchronize();

  popcli();
}