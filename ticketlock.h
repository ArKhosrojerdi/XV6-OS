// Mutual exclusion lock.
struct ticketlock
{
  int ticket;        // current ticket number being served
  int turn;          // next ticket number to be given
  struct proc *proc; // process currently holding the lock
  struct cpu *cpu;   // The cpu holding the lock.
};