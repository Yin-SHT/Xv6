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

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];

static int first = 1;

void
kinit()
{
  char lkname[16];

  for (int i = 0; i < NCPU; i ++) {
    snprintf(lkname, 16, "kmem_%d", i);
    initlock(&kmem[i].lock, lkname);
  }

  freerange(end, (void*)PHYSTOP);
  first = 0;
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;
  static int id = 0;
  int hart;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  // Only run in initialization
  if (first) {
    acquire(&kmem[id].lock);
    r->next = kmem[id].freelist;
    kmem[id].freelist = r;
    release(&kmem[id].lock);
    id = (id + 1) % NCPU;
  } else {
    push_off();

    hart = cpuid();
    acquire(&kmem[hart].lock);
    r->next = kmem[hart].freelist;
    kmem[hart].freelist = r;
    release(&kmem[hart].lock);

    pop_off();
  }
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  int hart;

  push_off();

  hart = cpuid();
  acquire(&kmem[hart].lock);
  r = kmem[hart].freelist;
  if(r) {
    kmem[hart].freelist = r->next;
    release(&kmem[hart].lock);
  } else {
    release(&kmem[hart].lock);

    for (int i = 0; i < NCPU; i ++) {
      acquire(&kmem[i].lock);
      r = kmem[i].freelist;
      if(r) {
        kmem[i].freelist = r->next;
        release(&kmem[i].lock);
        break;
      }
      release(&kmem[i].lock);
    }
  }

  pop_off();

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
