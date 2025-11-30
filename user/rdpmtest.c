#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  char *addr = sbrk(0);   // dirección actual del heap
  if (sbrk(4096) == (void *)-1) {
    printf("sbrk failed\n");
    exit(1);
  }

  // escribir antes de proteger
  addr[0] = 'Z';
  printf("antes de proteger: %c\n", addr[0]);

  // proteger contra lectura
  if (mrdprotect(addr, 1) < 0) {
    printf("mrdprotect falló\n");
    exit(1);
  }

  // sin tocar addr[0] aquí (ni leer ni escribir)

  // restaurar permisos de lectura
  if (munrdprotect(addr, 1) < 0) {
    printf("munrdprotect falló\n");
    exit(1);
  }

  // ahora la lectura debería funcionar de nuevo
  char c = addr[0];
  printf("después de munrdprotect, valor leído: %c\n", c);

  exit(0);
}
