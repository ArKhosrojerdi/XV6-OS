// Ticket locks

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "ticketlock.h"

int ticket = -1;
int q[100];
int qc = 0;
int head = 0;
int tail = 0;

void initTicketlock(struct ticketlock *lk)
{
  lk->ticket = 0;
  lk->turn = 0;
  lk->proc = 0;
  lk->cpu = 0;
}

void acquireTicketlock(struct ticketlock *lk)
{
  int currentTicket;
  pushcli(); // disable interrupts to avoid deadlock.
  currentTicket = fetch_and_add(&lk->ticket, 1);

  while (lk->turn != currentTicket);

  __sync_synchronize();
  lk->cpu = mycpu();
  lk->proc = myproc();
}

void releaseTicketlock(struct ticketlock *lk)
{
  if (!holdingTicketlock(lk))
    panic("release");

  lk->proc = 0;
  lk->cpu = 0;
  fetch_and_add(&lk->turn, 1);
  __sync_synchronize();
  popcli();
}

int holdingTicketlock(struct ticketlock *lk)
{
  if (lk->proc == myproc() && lk->turn != lk->ticket)
    return 1;
  return 0;
}