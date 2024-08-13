#include "kernel/param.h"
#include "kernel/fcntl.h"
#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/riscv.h"
#include "kernel/fs.h"
#include "user/user.h"

char buf[BSIZE];
char tmp[2 * PGSIZE];

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
  const char * const f = "mmap.dur";

  makefile(f);
  if ((fd = open(f, O_RDWR)) == -1)
    err("open (1)");

  int start = uptime();

  for (int i = 0; i < 10000; i ++) {
    write(fd, tmp, 2 * PGSIZE);
  }

  int end = uptime();

  printf("write: %d\n", end - start);

  close(fd);

  exit(0);
}

// 10