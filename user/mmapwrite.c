#include "kernel/param.h"
#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/riscv.h"
#include "kernel/fs.h"
#include "user/user.h"

#define MAP_FAILED ((char *) -1)

char buf[BSIZE];
char tmp[BSIZE];

char *testname = "???";

void
err(char *why)
{
  printf("mmaptest: %s failed: %s, pid=%d\n", testname, why, getpid());
  exit(1);
}

void
makefile(const char *f)
{
  int i;
  int n = PGSIZE/BSIZE;

  unlink(f);
  int fd = open(f, O_WRONLY | O_CREATE);
  if (fd == -1)
    err("open");
  memset(buf, 'A', BSIZE);
  // write 1.5 page
  for (i = 0; i < n + n; i++) {
    if (write(fd, buf, BSIZE) != BSIZE)
      err("write 0 makefile");
  }
  if (close(fd) == -1)
    err("close");
}

int
main(int argc, char *argv[])
{
  int fd;
  char tmp __attribute__((unused));
  const char * const f = "mmap.dur";

  makefile(f);
  if ((fd = open(f, O_RDWR)) == -1)
    err("open (1)");


  char *p = mmap(0, PGSIZE*2, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (p == MAP_FAILED)
    err("mmap (1)");

  int start = uptime();

  for (int k = 0; k < 10000; k ++) {
    for (int i = 0; i < 2 * PGSIZE; i ++) {
      p[i] = 0;
    }
  }

  if (munmap(p, PGSIZE*2) == -1)
    err("munmap (2)");

  int end = uptime();

  printf("mmap: %d\n", end - start);

  close(fd);
  exit(0);
}

// 1