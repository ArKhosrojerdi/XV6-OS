#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include "ticketlock.h"

struct
{
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

struct
{
  struct ticketlock tlock;
  struct proc proc[NPROC];
} ttable;

struct ticketlock mutex1;
struct ticketlock mutex2;
int rwcounter;
int readersCount;

static struct proc *initproc;

int nextpid = 1;
int sysc[40] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
extern void forkret(void);
extern void trapret(void);
int chCounter = 0; // counter for holding index for children array
int ch[1000];      // children array

static void wakeup1(void *chan);

void pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int cpuid()
{
  return mycpu() - cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu *
mycpu(void)
{
  int apicid, i;

  if (readeflags() & FL_IF)
    panic("mycpu called with interrupts enabled\n");

  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i)
  {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc *
myproc(void)
{
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc *
allocproc(void)
{
  struct proc *p;
  struct proc *p1; // a process for finding minimum calculatedPriority

  char *sp;

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == UNUSED)
      goto found;
  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;
  p->priority = 5;
  p->creationTime = ticks;
  p->sleepingTime = 0;
  p->readyTime = 0;
  p->runningTime = 0;
  p->zombieTime = 0;
  // p->ticket_no = ticket;
  int minpr = 1000000; // minimum priority saves in this variable

  // check emptiness of ptable and minimum priority
  for (p1 = ptable.proc + 2; p1 < &ptable.proc[NPROC]; p1++)
  {
    // minimum calculatedPriority among all processes
    if (p1->calculatedPriority < minpr)
      minpr = p1->calculatedPriority;
  }
  // min{5, calculatedPriority among all processes.
  if (minpr < 5)
    p->calculatedPriority = minpr;
  else
    p->calculatedPriority = 5;

  release(&ptable.lock);

  // Allocate kernel stack.
  if ((p->kstack = kalloc()) == 0)
  {
    p->state = UNUSED;
    p->priority = 0;
    p->calculatedPriority = 0;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe *)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint *)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context *)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();

  initproc = p;
  if ((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0; // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);
  p->state = RUNNABLE;
  p->tkRdy = ticks;
  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if (n > 0)
  {
    if ((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  else if (n < 0)
  {
    if ((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to .
int fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if ((np = allocproc()) == 0)
  {
    return -1;
  }

  // Copy process state from proc.
  if ((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0)
  {
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    np->priority = 0; // set priority to 0 after terminating
    np->calculatedPriority = 0;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for (i = 0; i < NOFILE; i++)
    if (curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);
  np->state = RUNNABLE;
  np->tkRdy = ticks;
  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if (curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for (fd = 0; fd < NOFILE; fd++)
  {
    if (curproc->ofile[fd])
    {
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->parent == curproc)
    {
      p->parent = initproc;
      if (p->state == ZOMBIE)
      {
        p->zombieTime = p->zombieTime + ticks - p->tkZmb;
        wakeup1(initproc);
      }
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  curproc->tkZmb = ticks;
  sched();
  cprintf("zombieExitError: \t %d \t %d\n", curproc->pid, curproc->calculatedPriority);
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for (;;)
  {
    // Scan through table looking for exited children.
    havekids = 0;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->parent != curproc)
        continue;
      havekids = 1;
      if (p->state == ZOMBIE)
      {
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        // set priorities to 0 after terminating
        p->priority = 0;
        p->calculatedPriority = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || curproc->killed)
    {
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock); //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void scheduler(void)
{
  struct proc *p;
  struct proc *p1;

  struct cpu *c = mycpu();
  c->proc = 0;

  for (;;)
  {
    // Enable interrupts on this processor.
    sti();

    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->state != RUNNABLE)
        continue;

      if (policy == 2)
      {
        struct proc *highP = ptable.proc;
        for (p1 = ptable.proc; p1 < &ptable.proc[NPROC]; p1++)
        {
          if (p1->state != RUNNABLE)
            continue;
          if (highP->calculatedPriority > p1->calculatedPriority)
            highP = p1;
        }

        // Switch to chosen process.  It is the process's job
        // to release ptable.lock and then reacquire it
        // before jumping back to us.
        p = highP; // process with highest priority is the next process
        p->calculatedPriority += p->priority;
        cprintf("pid: %d \t priority: %d \t cpr: %d\n", p->pid,p->priority, p->calculatedPriority);
      }
      p->readyTime = p->readyTime + ticks - p->tkRdy;
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;
      p->tkRun = ticks;
      swtch(&(c->scheduler), p->context);
      switchkvm();

      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
  }
}

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void sched(void)
{
  int intena;
  struct proc *p = myproc();

  if (!holding(&ptable.lock))
    panic("sched ptable.lock");
  if (mycpu()->ncli != 1)
    panic("sched locks");
  if (p->state == RUNNING)
    panic("sched running");
  if (readeflags() & FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void yield(void)
{
  acquire(&ptable.lock); //DOC: yieldlock
  myproc()->state = RUNNABLE;
  myproc()->tkRdy = ticks;
  myproc()->runningTime = myproc()->runningTime + ticks - myproc()->tkRun;
  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first)
  {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if (p == 0)
    panic("sleep");

  if (lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if (lk != &ptable.lock)
  {                        //DOC: sleeplock0
    acquire(&ptable.lock); //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;
  p->tkSlp = ticks;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if (lk != &ptable.lock)
  { //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if (p->state == SLEEPING && p->chan == chan)
    {
      p->sleepingTime = p->sleepingTime + (ticks - p->tkSlp);
      p->state = RUNNABLE;
      p->tkRdy = ticks;
    }
}

// Wake up all processes sleeping on chan.
void wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->pid == pid)
    {
      p->killed = 1;
      // Wake process from sleep if necessary.
      if (p->state == SLEEPING)
      {
        p->sleepingTime = p->sleepingTime + (ticks - p->tkSlp);
        p->state = RUNNABLE;
        p->tkRdy = ticks;
      }
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void procdump(void)
{
  static char *states[] = {
      [UNUSED] "unused",
      [EMBRYO] "embryo",
      [SLEEPING] "sleep ",
      [RUNNABLE] "runble",
      [RUNNING] "run   ",
      [ZOMBIE] "zombie"};
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->state == UNUSED)
      continue;
    if (p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if (p->state == SLEEPING)
    {
      getcallerpcs((uint *)p->context->ebp + 2, pc);
      for (i = 0; i < 10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

void iterateProcesses(int pid)
{
  struct proc *p;
  int i, flag;
  acquire(&ptable.lock);
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    flag = 0;
    if (p->pid != 0)
    {
      if (p->parent->pid == pid)
      {
        for (i = 0; i < chCounter; i++)
        {
          if (p->pid == ch[i])
          {
            cprintf("%d, %d\n", p->pid, p->parent->pid);
            flag = 1;
            break;
          }
        }
        if (!flag)
        {
          ch[chCounter] = p->pid;
          chCounter++;
        }
      }
    }
  }
  release(&ptable.lock);
}

void printPIDString(void)
{
  int k, ndigit = 10;
  int string = 0;
  for (k = 0; k < chCounter; k++)
    if (ch[k] >= 10)
    {
      ndigit *= 10;
      break;
    }

  k = chCounter - 1;
  for (; k >= 0; k--)
    string = ch[k] + (string * ndigit);

  cprintf("%d\n", string);
}

int getCount(int scno)
{
  cprintf("%d\n", sysc[scno - 1]);
  return 23;
}

int cps(void)
{
  struct proc *p;
  sti();
  acquire(&ptable.lock);
  cprintf("name \t pid \t state \t \t priority \t cpr\n");
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if (p->state == SLEEPING)
      cprintf("%s \t %d \t SLEEPING \t %d \t \t %d\n", p->name, p->pid, p->priority, p->calculatedPriority);
    else if (p->state == RUNNING)
      cprintf("%s \t %d \t RUNNING \t %d \t \t %d\n", p->name, p->pid, p->priority, p->calculatedPriority);
    else if (p->state == RUNNABLE)
      cprintf("%s \t %d \t RUNNABLE \t %d \t \t %d\n", p->name, p->pid, p->priority, p->calculatedPriority);
  }
  release(&ptable.lock);

  return 24;
}

int changePriority(int priority)
{
  struct proc *p = myproc();
  p->priority = priority;
  cprintf("priority: %d \t pid: %d\n", p->priority, p->pid);
  exit();
  return 1;
}

int changePolicy(int nPolicy)
{
  policy = nPolicy;
  cprintf("%d\n", policy);
  exit();
  return 25;
}

int waitForChild(void)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for (;;)
  {
    // Scan through table looking for exited children.
    havekids = 0;
    for (p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    {
      if (p->parent != curproc)
        continue;
      havekids = 1;
      if (p->state == ZOMBIE)
      {
        p->zombieTime = p->zombieTime + ticks - p->tkZmb;
        p->terminationTime = ticks;
        cprintf("For Process With ID= %d We Have\n", p->pid);
        cprintf("Creation    Time Equals: %d\n", p->creationTime);
        cprintf("Termination Time Equals: %d\n", p->terminationTime);
        cprintf("Sleeping    Time Equals: %d\n", p->sleepingTime);
        cprintf("ReadyTime   Time Equals: %d\n", p->readyTime);
        cprintf("Running     Time Equals: %d\n", p->runningTime);
        cprintf("Aggregation Time Equals: %d\n", p->runningTime + p->readyTime + p->sleepingTime);
        cprintf("Turnaround  Time Equals: %d\n", (p->terminationTime - p->creationTime - p->zombieTime));
        cprintf("Zombie      Time Equals: %d\n", p->zombieTime);
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        p->priority = 0;
        p->calculatedPriority = 0;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if (!havekids || curproc->killed)
    {
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock); //DOC: wait-sleep
  }
}

void ticketlockInit(void)
{
  initTicketlock(&ttable.tlock);
}

int ticketlockTest(void)
{
  acquireTicketlock(&ttable.tlock);
  int i;
  for (i = 0; i < 10; i++)
    cprintf("myproc.pid: %d \t counter: %d\n", myproc()->pid, i);
  releaseTicketlock(&ttable.tlock);
  i = (&ttable.tlock)->ticket;
  return i;
}

void rwinit(void)
{
  initTicketlock(&mutex1);
  initTicketlock(&mutex2);

  rwcounter = 0;
  readersCount = 0;
}

int rwtest(int rw)
{

  if (rw == 0)
  {
    int readdata = 100;

    acquireTicketlock(&mutex2);
    readersCount++;
    if (readersCount == 1)
    {
      acquireTicketlock(&mutex1);
    }
    releaseTicketlock(&mutex2);

    //REAAAD
    readdata = rwcounter;
    //REAAAD

    acquireTicketlock(&mutex2);
    readersCount--;
    if (readersCount == 0)
    {
      releaseTicketlock(&mutex1);
    }
    releaseTicketlock(&mutex2);

    return readdata;
  }
  else if (rw == 1)
  {

    acquireTicketlock(&mutex1);

    //WRITE
    rwcounter++;
    //WRITE

    releaseTicketlock(&mutex1);

    return 1;
  }
  else
  {
    cprintf("wrong format!!!");
    return -1;
  }
  return 1;
}
