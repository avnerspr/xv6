// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);
void ref_array_init(uint64 start, uint64 final);
void * unref_kalloc(void);
void unref_kfree(void *pa);


extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct reference_ctr {
  uint64 start_ph;
  uint64 legnth;
  char*  ctrs_array;
};

static struct reference_ctr pages_refcount = {.legnth = 0, .start_ph = 0, .ctrs_array = 0};

#define PA2PAGEREF(pa) (pages_refcount.ctrs_array[((uint64)pa - pages_refcount.start_ph) / PGSIZE])

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  printf("start kinit\n");
  initlock(&kmem.lock, "kmem");
  freerange(end, (void*)PHYSTOP);
  ref_array_init((uint64)end, (uint64)PHYSTOP);
}

void ref_array_init(uint64 start, uint64 final){
  printf("starting ref_array_init\n");
  pages_refcount.start_ph = PGROUNDUP(start);
  pages_refcount.legnth = (PGROUNDDOWN(final) - pages_refcount.start_ph) / PGSIZE;
  for (uint64 i = 0; i < PGROUNDUP(pages_refcount.legnth) / PGSIZE; i++){
      pages_refcount.ctrs_array = (char *)unref_kalloc();  //& the pages get kalloced by order from ones with large pa to small. we inverse the order.
      memset(pages_refcount.ctrs_array, 1, PGSIZE);   //& init everithing to be ready for first kalloc  
  }
  printf("finished ref_array_init\n");
}

void
freerange(void *pa_start, void *pa_end)
{
  printf ("start freerange\n");
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    unref_kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
 
void
unref_kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

//  reduce page reference count by 1. if zero then free.
void kfree(void * pa){

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP || \
      (uint64)pa < pages_refcount.start_ph){
          
          panic("kfree");
      }
  
  char ref_ctr = --PA2PAGEREF(pa);
  if (ref_ctr >= NPROC)
      panic("kfree");
  if (ref_ctr > 0 && ref_ctr)
      return;
  PA2PAGEREF(pa) = 1;
  memset(pa, 3, PGSIZE);
  struct run *r = (struct run *)pa;
  acquire(&kmem.lock);
  r->next = kmem.freelist;
  kmem.freelist = r;
  release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
unref_kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}


void * kalloc(void){
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
  {
    memset((char*)r, 5, PGSIZE); // fill with junk
    if ((uint64)r < pages_refcount.start_ph || (uint64)r >= pages_refcount.start_ph + pages_refcount.legnth * PGSIZE)
        panic("kalloc");
    PA2PAGEREF(r) = 1;
  }
  return (void*)r;
}

void add_ref(void * pa){
    if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP || \
        PA2PAGEREF(pa) >= NPROC || PA2PAGEREF(pa) == 0)
            panic("add_ref");
  
  PA2PAGEREF(pa)++;  
}
