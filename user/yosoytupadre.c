#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  printf("yo=%d, ppid=%d\n", getpid(), getppid());

  for(int k = 0; k <= 4; k++)
    printf("ancestor(%d) = %d\n", k, getancestor(k));

  int pid = fork();
  if(pid < 0){
    printf("fork fallo\n");
    exit(1);
  }
  if(pid == 0){
    printf("Hijo: yo=%d, ppid=%d\n", getpid(), getppid());
    for(int k = 0; k <= 4; k++)
      printf("Hijo: ancestor(%d) = %d\n", k, getancestor(k));
    exit(0);
  } else {
    wait(0);
  }
  exit(0);
}
