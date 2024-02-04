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

struct bucket {
  struct buf head;
  struct spinlock lock;
  uint index;
};

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct bucket buckets[NBUCKET];
  

} bcache;


// Find the least recently used (LRU) unused buffer.
// when called the process does not hold any relevent locks
// when returns the process holds the lock of the unused buffer bucket
struct buf * get_unused_buffer(void) {
  struct buf * b;
  struct buf * unusedbuf = 0;
  struct bucket * buck;
  struct bucket * unused_bucket = 0;
  
  for (buck = bcache.buckets; buck < bcache.buckets+NBUCKET; buck++)
  {
    acquire(&buck->lock);
    int unchanged = 1;
    for (b = buck->head.next; b != &buck->head; b = b->next)
    {
      if (b->refcnt > 0) {
        continue;
      }
      if (unusedbuf == 0 || unusedbuf->unusedtime > b->unusedtime) {
        unusedbuf = b;
        unchanged = 0;
      }
    }
    if (unchanged) {
      release(&buck->lock);
    }
    else {
      if (unused_bucket != 0) {
        release(&unused_bucket->lock);
      }
      unused_bucket = buck;
    }
  }
  if (unusedbuf == 0) {
      panic("bget: no buffers");
  }
  return unusedbuf;
}


void
binit(void)
{
  struct buf *b;
  struct bucket *buck;

  initlock(&bcache.lock, "bcache");

  // Create linked list of free buffers


  // Create linked list in each bucket
  for (buck = bcache.buckets; buck < bcache.buckets+NBUCKET; buck++)
  {
    buck->head.prev = &buck->head;
    buck->head.next = &buck->head;
    buck->index = buck - bcache.buckets;
    initlock(&buck->lock, "bcachebucket");
  }

  for(b = bcache.buf; b < bcache.buf+NBUF; b++){
    buck = &bcache.buckets[(b - bcache.buf) % NBUCKET];
    b->bucketindex = buck - bcache.buckets;
    b->next = buck->head.next;
    b->prev = &buck->head;
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
  struct bucket * bufbucket = &bcache.buckets[blockno % NBUCKET];


  acquire(&bufbucket->lock);
  // Is the block already cached?
  for(b = bufbucket->head.next; b != &bufbucket->head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bufbucket->lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bufbucket->lock);
  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  struct buf * unusedbuf = get_unused_buffer();
  
  // extracting unusedbuf from bucket
  unusedbuf->next->prev = unusedbuf->prev;
  unusedbuf->prev->next = unusedbuf->next;
  release(&bcache.buckets[unusedbuf->bucketindex].lock);

  // insert to correct bucket, change block metadata and return block
  acquire(&bufbucket->lock);
  unusedbuf->next = bufbucket->head.next;
  unusedbuf->prev = &bufbucket->head;
  bufbucket->head.next->prev = unusedbuf;
  bufbucket->head.next = unusedbuf;

  unusedbuf->bucketindex = bufbucket->index;

  unusedbuf->dev = dev;
  unusedbuf->blockno = blockno;
  unusedbuf->valid = 0;
  unusedbuf->refcnt = 1;
  release(&bufbucket->lock);
  acquiresleep(&unusedbuf->lock);
  return unusedbuf;
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


  releasesleep(&b->lock);
  struct bucket * buck = &bcache.buckets[b->blockno % NBUCKET];

  acquire(&buck->lock);
  b->refcnt--;
  acquire(&tickslock);
  b->unusedtime = ticks;
  release(&tickslock);
  release(&buck->lock);
}

void
bpin(struct buf *b) {
  struct bucket * buck = &bcache.buckets[b->blockno % NBUCKET];
  acquire(&buck->lock);
  b->refcnt++;
  release(&buck->lock);
}

void
bunpin(struct buf *b) {
  struct bucket * buck = &bcache.buckets[b->blockno % NBUCKET];
  acquire(&buck->lock);
  b->refcnt--;
  release(&buck->lock);
}


