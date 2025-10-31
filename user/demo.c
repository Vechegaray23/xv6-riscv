#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

static void burn(unsigned long iters) {
  volatile unsigned long x = 0;
  for (unsigned long i = 0; i < iters; i++) x += i;
}

int
main(int argc, char *argv[])
{
  int N = 5;                 // crea 5 procesos hijo
  unsigned long work = 120000000UL;

  printf("demo: creando %d hijos con distintos tickets\n", N);

  for (int i = 0; i < N; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("demo: fork fallo\n");
      exit(1);
    }
    if (pid == 0) {
      int t = 50 * (i + 1);           // 50, 100, 150, 200, 250
      settickets(t);
      printf("hijo %d: tickets=%d, empezando carga...\n", getpid(), t);
      burn(work);
      printf("hijo %d: fin\n", getpid());
      exit(0);
    }
  }

  // padre espera a todos
  for (int i = 0; i < N; i++) wait(0);

  printf("demo: listo\n");
  exit(0);
}
