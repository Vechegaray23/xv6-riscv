#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  char *addr;

  // pedir una página de memoria
  addr = (char *)sbrk(4096);
  if (addr == (char *)-1) {
    printf("sbrk failed\n");
    exit(1);
  }

  // escribir antes de proteger
  addr[0] = 'Z';
  printf("before protect: %c\n", addr[0]);

  // proteger contra lectura
  if (mrdprotect(addr, 1) < 0) {
    printf("mrdprotect failed\n");
    exit(1);
  }

  printf("after mrdprotect: wrote A\n");
  // escribir debería seguir funcionando
  addr[0] = 'A';

  printf("now trying to read (should cause a page fault and kill the process)\n");

  // ESTA LECTURA debería causar fallo de página si tu syscall funciona bien
  printf("read value: %c\n", addr[0]);

  // si llegamos aquí, la protección falló
  printf("ERROR: read succeeded, protection not working\n");

  // intentar restaurar (en teoría nunca llega acá si el kernel mata el proceso)
  if (munrdprotect(addr, 1) < 0) {
    printf("munrdprotect failed\n");
  }

  exit(0);
}
