// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"
#include "proc.h"

char* KMEM_NAME[NCPU] = {
  [0] "kmem0",
  [1] "kmem1",
  [2] "kmem2",
  [3] "kmem3",
  [4] "kmem4",
  [5] "kmem5",
  [6] "kmem6",
  [7] "kmem7" 
};

void freerange(void *pa_start, void *pa_end);
void kfree_tocpu(void *pa, int cpu_index);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

void
kinit()
{
  for (int i = 0; i < NCPU; i++)
      initlock(&kmem[i].lock, KMEM_NAME[i]);
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  uint ctr = 0;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
  {
    kfree_tocpu(p, ctr);
    ctr++;
    ctr %= NCPU;
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  uint mycpu_id;
  push_off();
  mycpu_id = cpuid();
  pop_off();
  kfree_tocpu(pa, mycpu_id);
}

void kfree_tocpu(void *pa, int cpu_index){
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree_toCpus");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem[cpu_index].lock);
  r->next = kmem[cpu_index].freelist;
  kmem[cpu_index].freelist = r;
  release(&kmem[cpu_index].lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  push_off();
  uint mycpu_index = cpuid();
  pop_off();
  for (uint ctr = 0; ctr < NCPU; ctr++)
  {
    uint index = (mycpu_index + ctr) % NCPU;  
    acquire(&kmem[index].lock);
    r = kmem[index].freelist;
    if(r)
      kmem[index].freelist = r->next;
    release(&kmem[index].lock);

    if(r)
    {
      memset((char*)r, 5, PGSIZE); // fill with junk
      return (void*)r;
    }
  }
  return 0; 

}



