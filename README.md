# Tarea 2 — Lottery Scheduling en xv6 (RISC-V)

## Integrantes
Vicente echegaray
Lucas Bahamondes

## Qué se implementó
1. **Campos nuevos en `struct proc`**:  
   - `int tickets;` — tickets del proceso (≥ 1).  
   - `uint64 cpu_slices;` — veces seleccionado por el scheduler.
2. **Inicialización por defecto** en `allocproc()`:
   - `tickets = 100`, `cpu_slices = 0`.
3. **Syscall `settickets(int n)`**:
   - Kernel: `sys_settickets` en `kernel/sysproc.c`.
   - Número: `SYS_settickets` en `kernel/syscall.h`.
   - Mapeo: prototipo + entrada en `kernel/syscall.c`.
   - Userland: prototipo en `user/user.h` y stub generado vía `user/usys.pl`.
4. **Scheduler por lotería** (`kernel/proc.c`):
   - Dos pasadas: sumar tickets de `RUNNABLE`, luego sortear y correr ganador.
   - RNG simple (`krand()`/`kroll()`).
   - Contador `cpu_slices++` en el ganador.
5. **Contabilidad**:
   - En `exit()`: `printf("proc %d (%s): tickets=%d, cpu_slices=%d\n", ...)`.
6. **Programa de prueba `demo`**:
   - `user/demo.c` + entrada en `UPROGS` del `Makefile`.

## Archivos modificados / añadidos
- `kernel/proc.h`: campos `tickets`, `cpu_slices`.
- `kernel/proc.c`: init en `allocproc()`, RNG helpers, cambio en `scheduler()`, print en `exit()`.
- `kernel/sysproc.c`: `sys_settickets`.
- `kernel/syscall.h`: `#define SYS_settickets 24`.
- `kernel/syscall.c`: `extern`, tabla `syscalls[]` (+ opcional `syscall_names[]`).
- `user/user.h`: `int settickets(int n);`
- `user/usys.pl`: agregado `settickets` para generar stub.
- `user/demo.c`: prueba.
- `Makefile`: agregado `$U/_demo` a `UPROGS`.

## Cómo compilar y correr
```sh
make qemu
# en el prompt de xv6:
demo
# salir: Ctrl–a, luego x
```

## Resultados esperados (ejemplo)
Cada proceso imprime al terminar algo como:
```
proc <pid> (demo): tickets=50,  cpu_slices=...
proc <pid> (demo): tickets=100, cpu_slices=...
...
```
Los `cpu_slices` deberían reflejar **aproximadamente** la proporción de tickets (mayor tickets → más slices).

## Decisiones y robustez
- Si `settickets(n)` recibe `n < 1`, se fuerza a `1`.
- Si un proceso `RUNNABLE` tiene `tickets < 1`, se corrige a `1` al sumar.
- Si `total == 0` (no hay `RUNNABLE`), el scheduler continúa el loop sin bloquear.
- RNG simple (LCG) suficiente para fines docentes.

## Dificultades y soluciones
- **Locks por proceso**: dos pasadas con `acquire/release` cortos para evitar mantener locks largos.
- **Generación de stub userland**: modificar `user/usys.pl` (no `usys.S` directo).
- **Makefile/UPROGS**: cuidado con las barras invertidas `\` (última entrada sin barra).

## Posibles mejoras
- Semilla del RNG basada en `ticks`.
- Métricas más finas (tiempo real de CPU).
- Tests adicionales con distintos `N` y cargas.

## Cómo reproducir la demo con otros tickets
Editar `user/demo.c` y cambiar `t = 50 * (i + 1);` o `N` y `work`.
