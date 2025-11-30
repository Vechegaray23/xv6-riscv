# Tarea 3: Protección de Lectura en XV6
Integrantes:
- Vicente Echegaray
- Lucas Bahamondes

## 1. Objetivo

El objetivo de esta tarea es extender XV6 para soportar protección de memoria a nivel de página, permitiendo marcar una región de memoria de usuario como **no legible** mediante dos nuevas llamadas al sistema:

- `int mrdprotect(void *addr, int len);`
- `int munrdprotect(void *addr, int len);`

`mrdprotect` deshabilita la lectura en un rango de páginas a partir de `addr`, mientras que `munrdprotect` restaura el permiso de lectura. La idea está motivada por escenarios donde se quiere manejar datos sensibles (por ejemplo, claves criptográficas) minimizando la posibilidad de lectura accidental o maliciosa.

---

## 2. Diseño general de la solución

La solución se implementa respetando la arquitectura de XV6, agregando:

1. **Interfaz en espacio de usuario**  
   - Declaraciones en `user/user.h`.
   - Stubs de syscall generados a partir de `user/usys.pl`.

2. **Nuevas entradas de syscall en el kernel**  
   - Números de syscall definidos en `kernel/syscall.h`.
   - Asociación número → función en `kernel/syscall.c`.
   - Implementación de `sys_mrdprotect` y `sys_munrdprotect` en `kernel/sysproc.c`.

3. **Lógica principal de protección en `vm.c`**  
   - Funciones internas del kernel:
     - `int do_mrdprotect(uint64 addr, int len);`
     - `int do_munrdprotect(uint64 addr, int len);`
   - Validación de parámetros y PTEs.
   - Modificación del bit `PTE_R` en cada PTE afectada.
   - Flush de la TLB con `sfence_vma()`.

4. **Programas de prueba en espacio de usuario**
   - `rdprotect_test`: verifica que el acceso a memoria protegida cause un fallo.
   - `rdpmtest`: verifica que `munrdprotect` restaura el permiso de lectura correctamente.

---

## 3. Cambios realizados en XV6

### 3.1. Interfaz de usuario

#### `user/user.h`

Se agregan los prototipos:

```c
int mrdprotect(void *addr, int len);
int munrdprotect(void *addr, int len);
```

Esto permite que cualquier programa de usuario pueda invocar las funciones como si fueran llamadas normales en C.

#### `user/usys.pl`

Se agregan las entradas para generar los stubs de syscall:

```perl
entry("mrdprotect");
entry("munrdprotect");
```

El script `usys.pl` genera automáticamente `user/usys.S`, que contiene el código ensamblador necesario para hacer el salto a modo kernel.

#### Programas de prueba (`user/rdprotect_test.c` y `user/rdpmtest.c`)

- `rdprotect_test`:
  - Reserva una página con `sbrk(4096)`.
  - Escribe un valor inicial en `addr[0]`.
  - Llama a `mrdprotect(addr, 1)`.
  - Intenta acceder nuevamente a la memoria; el acceso provoca un fallo de página y el kernel mata al proceso, por lo que nunca se debería imprimir el mensaje de éxito de lectura.

- `rdpmtest`:
  - Reserva una página.
  - Escribe un valor inicial (`'Z'`).
  - Llama a `mrdprotect(addr, 1)`.
  - Llama a `munrdprotect(addr, 1)` sin tocar la memoria entre medio.
  - Vuelve a leer `addr[0]` y verifica que la lectura funciona, mostrando el valor almacenado.

#### `Makefile`

Se agregan los binarios de usuario a la variable `UPROGS`:

```make
$U/_rdprotect_test\
$U/_rdpmtest\
```

para que sean incluidos en la imagen de sistema de archivos (`fs.img`).

---

### 3.2. Nuevos números de syscall

Archivo: `kernel/syscall.h`

Se definen los nuevos identificadores (números concretos dependen del último valor existente en el archivo):

```c
#define SYS_mrdprotect   XX
#define SYS_munrdprotect YY
```

donde `XX` y `YY` son dos números consecutivos inmediatamente después del último `SYS_...` existente.

---

### 3.3. Registro de las syscalls en el kernel

Archivo: `kernel/syscall.c`

1. Se declaran las funciones de kernel:

```c
extern uint64 sys_mrdprotect(void);
extern uint64 sys_munrdprotect(void);
```

2. Se enlazan los números de syscall con sus manejadores en el arreglo `syscalls[]`:

```c
[SYS_mrdprotect]   sys_mrdprotect,
[SYS_munrdprotect] sys_munrdprotect,
```

---

### 3.4. Implementación de los manejadores de syscall

Archivo: `kernel/sysproc.c`

Se agregan las funciones:

```c
uint64
sys_mrdprotect(void)
{
  uint64 addr;
  int len;

  argaddr(0, &addr);
  argint(1, &len);

  return do_mrdprotect(addr, len);
}

uint64
sys_munrdprotect(void)
{
  uint64 addr;
  int len;

  argaddr(0, &addr);
  argint(1, &len);

  return do_munrdprotect(addr, len);
}
```

Estas funciones se encargan de:

- Obtener los argumentos de la syscall desde el espacio de usuario (`addr` y `len`).
- Delegar la lógica al nivel de memoria virtual (`vm.c`) mediante `do_mrdprotect` y `do_munrdprotect`.

---

### 3.5. Prototipos en `defs.h`

Archivo: `kernel/defs.h`

En la sección correspondiente a `vm.c` se agregan:

```c
int do_mrdprotect(uint64 addr, int len);
int do_munrdprotect(uint64 addr, int len);
```

Esto permite que otras partes del kernel llamen a estas funciones si fuera necesario.

---

## 4. Lógica de protección en `vm.c`

Archivo: `kernel/vm.c`

Se incluye `proc.h` para poder acceder al proceso actual:

```c
#include "proc.h"
```

### 4.1. `do_mrdprotect(uint64 addr, int len)`

Responsable de marcar como **no legible** el rango de direcciones:

- Rango: `[addr, addr + len * PGSIZE)`.
- Se opera a nivel de páginas.

Pasos principales:

1. **Validaciones iniciales**

```c
if (len <= 0)
  return -1;

if (addr % PGSIZE != 0)
  return -1;

struct proc *p = myproc();
if (p == 0)
  return -1;
```

- `len` debe ser estrictamente positivo.
- `addr` debe estar alineada al tamaño de página (`PGSIZE`).
- Se obtiene el proceso actual (`myproc()`).

2. **Recorrido y validación de todas las páginas**

Para cada página `i` en el rango:

```c
va = addr + i * PGSIZE;

if (va >= p->sz)
  return -1;                    // fuera del espacio de usuario del proceso

pte_t *pte = walk(p->pagetable, va, 0);
if (pte == 0)
  return -1;                    // no hay entrada de página

if ((*pte & PTE_V) == 0)
  return -1;                    // PTE no válida

if ((*pte & PTE_U) == 0)
  return -1;                    // no es memoria de usuario (posible kernel)
```

Con estas comprobaciones se asegura que:

- Todas las direcciones estén dentro del espacio de usuario del proceso.
- Todas las páginas estén mapeadas (`PTE_V`).
- No se toca memoria del kernel (`PTE_U` debe estar presente para páginas de usuario).

3. **Modificación de los PTEs**

Si todas las páginas pasan las validaciones, se realiza un segundo recorrido donde se limpia el bit de lectura:

```c
*pte &= ~PTE_R;
```

Esto deshabilita el permiso de lectura (`PTE_R`) sin alterar el resto de los bits de la PTE (por ejemplo, `PTE_W`, `PTE_X`, `PTE_U`, `PTE_V`).

4. **Flush de TLB**

Finalmente se llama a:

```c
sfence_vma();
```

para asegurar que los cambios en las PTEs se reflejen en la TLB de la CPU.

5. **Valor de retorno**

- `0` si todas las páginas fueron procesadas correctamente.
- `-1` ante cualquier error de validación.

---

### 4.2. `do_munrdprotect(uint64 addr, int len)`

Esta función es simétrica a `do_mrdprotect`:

1. Realiza las mismas validaciones de `len`, alineamiento de `addr`, y rango de direcciones.
2. Verifica que todas las páginas:
   - Están bajo `p->sz`.
   - Tienen una PTE válida (`PTE_V`).
   - Pertenecen al espacio de usuario (`PTE_U`).

3. En el segundo recorrido, en lugar de limpiar el bit, lo activa:

```c
*pte |= PTE_R;
```

4. Vuelve a llamar a `sfence_vma()` para actualizar la TLB.
5. Retorna `0` en éxito y `-1` en caso de error.

---

## 5. Manejo de errores

Ambas funciones deben devolver `-1` en los siguientes casos:
- `addr` no está alineada a página.
- `len <= 0`.
- Alguna dirección no pertenece al espacio de usuario del proceso.
- Alguna página del rango no tiene `PTE_V` (no está mapeada).
- La operación implicaría modificar memoria del kernel.

Estos casos se controlan con:

- Chequeo de alineación: `addr % PGSIZE != 0`.
- Chequeo de longitud: `len <= 0`.
- Chequeo de rango: `va >= p->sz`.
- Chequeo de PTE: `pte == 0 || (*pte & PTE_V) == 0`.
- Chequeo de memoria de usuario: `(*pte & PTE_U) == 0`.

En cualquiera de estas condiciones, la función retorna inmediatamente `-1`. Como la verificación se hace en un primer recorrido antes de modificar las PTEs, se evita dejar el sistema en un estado inconsistente.

---

## 6. Pruebas realizadas

### 6.1. `rdprotect_test`

Secuencia:

1. `addr = sbrk(4096);`
2. `addr[0] = 'Z';` (escritura inicial).
3. `mrdprotect(addr, 1);`
4. Intento posterior de acceso a la memoria protegida (lectura en el programa del enunciado).

Comportamiento observado:

- El kernel imprime un mensaje del estilo:

  ```text
  usertrap(): unexpected scause 0xf pid=...
              stval=0x4000
  ```

- El proceso `rdprotect_test` es terminado.
- No se imprime el mensaje que indica lectura exitosa.

Esto confirma que al intentar acceder a la página marcada por `mrdprotect`, se produce un fallo de página y el kernel mata al proceso.

### 6.2. `rdpmtest` (prueba de `munrdprotect`)

Secuencia:

1. Se reserva una página con `sbrk`.
2. Se escribe un valor inicial en `addr[0]`.
3. Se llama a `mrdprotect(addr, 1)`.
4. Se llama a `munrdprotect(addr, 1)` sin acceder a la memoria entre medio.
5. Se lee `addr[0]` y se imprime el valor.

Comportamiento observado:

- El programa termina normalmente.
- El valor leído después de `munrdprotect` coincide con el esperado.
- Esto comprueba que el bit `PTE_R` se restaura correctamente y que la página vuelve a ser legible.

---

## 7. Comentarios sobre la arquitectura RISC-V y limitaciones

El enunciado propone un modelo de memoria “solo escritura” (write-only) en el que idealmente se podría escribir en la página pero no leerla. En la práctica, lo importante para la tarea es:

- **Desde el punto de vista del proceso de usuario**, cualquier acceso a la región protegida (especialmente la lectura) provoca un fallo de página y el proceso es terminado por el kernel.
- `munrdprotect` permite volver a un estado “normal” (con lectura habilitada) para las páginas previamente protegidas.

La implementación cumple con el requisito de **impedir la lectura** de las páginas protegidas y permite restaurar ese permiso cuando el proceso lo requiera.

---

## 8. Cómo compilar y ejecutar

Para compilar desde cero y arrancar XV6:

```bash
make clean
make qemu
```

Una vez dentro de XV6:

- Para probar la protección de lectura:

  ```bash
  rdprotect_test
  ```

- Para probar la restauración del permiso de lectura:

  ```bash
  rdpmtest
  ```

En ambos casos, el comportamiento observado coincide con el diseño descrito arriba.
