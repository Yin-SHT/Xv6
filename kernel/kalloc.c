// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

#define LENGTH ((PHYSTOP - KERNBASE) / PGSIZE)

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

struct count {
  struct spinlock lock;
  int count[LENGTH];
} refcnt;   // reference count

int r_count(uint64 pa) {
  int idx = (pa - KERNBASE) >> PGSHIFT;
  if (idx >= LENGTH)
    panic("panic: r_count");
  return refcnt.count[idx];
}

// decrease reference count
void d_count(uint64 pa) {
  int idx = (pa - KERNBASE) >> PGSHIFT;
  if (idx >= LENGTH)
    panic("panic: r_count");
  refcnt.count[idx] --;
}

// increase reference count
void i_count(uint64 pa) {
  int idx = (pa - KERNBASE) >> PGSHIFT;
  if (idx >= LENGTH)
    panic("panic: r_count");
  
  acquire(&refcnt.lock);
  refcnt.count[idx] ++;
  release(&refcnt.lock);
}

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&refcnt.lock, "refcnt");
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE) {
    i_count((uint64)p);
    kfree(p);
  }
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  acquire(&refcnt.lock);
  d_count((uint64)pa);
  if(r_count((uint64)pa) == 0){
    release(&refcnt.lock);

    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  } else
    release(&refcnt.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
    kmem.freelist = r->next;
  release(&kmem.lock);

  if(r)
    i_count((uint64)r);

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
int
cowalloc(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;
  uint flags;
  void *mem;
  int cnt;

  if(va >= MAXVA)
    return -1;
  if((pte = walk(pagetable, va, 0)) == 0)
    return -1;
  if((*pte & PTE_V) == 0)
    return -1;
  if((*pte & PTE_F) == 0)
    return -1;
  if((*pte & PTE_W) != 0)
    return -1;
  
  pa = PTE2PA(*pte);
  flags = PTE_FLAGS(*pte);
  acquire(&refcnt.lock);
  cnt = r_count((uint64)pa);
  release(&refcnt.lock);

  if(cnt == 1){
    *pte = (*pte & ~PTE_F) | PTE_W;
  } else {
    flags = (((flags & ~PTE_F) | PTE_W) & ~PTE_V);
    if((mem = kalloc()) == 0)
      return -1;
    memmove(mem, (char*)pa, PGSIZE);
    uvmunmap(pagetable, PGROUNDDOWN(va), 1, 1);
    if(mappages(pagetable, PGROUNDDOWN(va), PGSIZE, (uint64)mem, flags) != 0){
      kfree(mem);
      return -1;
    }
  }

  return 0;
}
