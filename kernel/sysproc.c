#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;


  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}


#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  uint64 va;
  int len;
  uint64 result_uaddr;
  argaddr(0, &va);
  argint(1, &len);
  argaddr(2, &result_uaddr);
  int page_acsessed;
  int byte_ctr;
  if (len > BYTEBITSNUM(PGSIZE))
      return -1;
  
  pagetable_t my_pagetable = myproc()->pagetable;
  char* res_mask = (char *)kalloc();
  for (byte_ctr = 0; byte_ctr < BYTEGROUNDUP(len); byte_ctr++)
      res_mask[byte_ctr] = 0;
  
  for (uint64 ctr = 0; ctr < len; ctr++)
  {
    if ((page_acsessed = check_acsessed(my_pagetable, va)) < 0){
      return -1;
    }
    ORBIT(res_mask[ctr / 8], ctr % 8, page_acsessed);
    va += PGSIZE;
  }

  copyout(my_pagetable, result_uaddr, res_mask, (len + 7)/8);
  kfree(res_mask);
  return 0;
}
#endif

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
