// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NBUCKET (13)

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct buf head;
} bcache;


struct bucket {
  struct buf head;
  struct spinlock lock;
};

struct bucket hashtable[NBUCKET];

void
binit(void)
{
  struct buf *b;
  struct bucket *buck;

  initlock(&bcache.lock, "bcache");

  for (buck = hashtable; buck < hashtable+NBUCKET; buck++)
  {
    initlock(&buck->lock, "bcache_bucket");
    buck->head.prev = &buck->head;
    buck->head.next = &buck->head;
  }
  // Create linked list of buffers
  buck = hashtable;
  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    b->next = buck->head.next;
    b->prev = &buck->head;
    b->refcnt = 0;
    initsleeplock(&b->lock, "buffer");
    buck->head.next->prev = b;
    buck->head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  struct bucket * buck = &hashtable[blockno % NBUCKET];


  acquire(&buck->lock);

  // Is the block already cached?
  for(b = buck->head.next; b != &buck->head; b = b->next)
  {
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&buck->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  struct buf *unused = 0;
  struct bucket * incbuck = buck;
  uint unused_bucket_index = 0;
  release(&incbuck->lock);
  acquire(&bcache.lock);
  for (buck = hashtable; buck < hashtable+NBUCKET; buck++)
  {
    acquire(&buck->lock);
    for(b = buck->head.next; b != &buck->head; b = b->next)
    {
      if (b->refcnt > 0)
          continue;
      if (unused == 0 || b->usedtime < unused->usedtime)
      {
          unused = b;
          unused_bucket_index = buck - hashtable;
      }
    }
    release(&buck->lock);
  }

  if (unused == 0)
      panic("bget: no buffers");

  acquire(&incbuck->lock);
  if (&hashtable[unused_bucket_index] != incbuck)
      acquire(&hashtable[unused_bucket_index].lock);
  unused->next->prev = unused->prev;
  unused->prev->next = unused->next;
  unused->next = incbuck->head.next;
  unused->prev = &incbuck->head;
  unused->prev->next = unused;
  unused->next->prev = unused;
  if (&hashtable[unused_bucket_index] != incbuck)
      release(&hashtable[unused_bucket_index].lock);
  
  unused->dev = dev;
  unused->blockno = blockno;
  unused->valid = 0;
  unused->refcnt = 1;
  release(&incbuck->lock);
  release(&bcache.lock);
  acquiresleep(&unused->lock);
  return unused;
  //  // Not cached.
  // // Recycle the least recently used (LRU) unused buffer.
  // for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
  //   if(b->refcnt == 0) {
  //     b->dev = dev;
  //     b->blockno = blockno;
  //     b->valid = 0;
  //     b->refcnt = 1;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //   }
  // }
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");
  struct bucket * buck = &hashtable[b->blockno % NBUCKET];
  releasesleep(&b->lock);
  acquire(&buck->lock);

  b->refcnt--;
  if (b->refcnt == 0)
  {
    acquire(&tickslock);
    b->usedtime = ticks;
    release(&tickslock);
  }
  release(&buck->lock);

  // acquire(&bcache.lock);
  // b->refcnt--;
  // if (b->refcnt == 0) {
  //   // no one is waiting for it.
  //   b->next->prev = b->prev;
  //   b->prev->next = b->next;
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
  
  // release(&bcache.lock);
}

void
bpin(struct buf *b) {
  struct bucket * buck = &hashtable[b->blockno % NBUCKET];
  acquire(&buck->lock);
  b->refcnt++;
  release(&buck->lock);
}

void
bunpin(struct buf *b) {
  struct bucket * buck = &hashtable[b->blockno % NBUCKET];
  acquire(&buck->lock);
  b->refcnt--;
  release(&buck->lock);
}


