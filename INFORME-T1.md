# Tarea 1 — `getppid` y `getancestor` en xv6-riscv

**Autores:** Vicente Echegaray; Lucas Bahamondes  
**Repositorio (rama):** `https://github.com/Vechegaray23/xv6-riscv/tree/vicente_echegaray_t1`  
**Fecha:** 21/09/2025

---

## 1. Enunciado (resumen propio)

Implementar dos nuevas llamadas al sistema en xv6-riscv:

- `int getppid(void)`: retorna el PID del proceso padre; si no existe, retorna `-1`.  
- `int getancestor(int n)`: retorna el PID del **n-ésimo ancestro** del proceso actual  
  (`n=0 → self`, `n=1 → padre`, `n=2 → abuelo`, …). Si no existe, retorna `-1`.

Además, crear un programa de usuario `yosoytupadre.c` que demuestre el funcionamiento e integrarlo al `Makefile`.

---

## 2. Diseño y decisiones

- **Convención de ancestros:**  
  - `n < 0` → `-1`.  
  - `n = 0` → PID del proceso actual.  
  - `n > 0` → subir por `parent` n veces; si en algún punto `parent == 0`, devolver `-1`.

- **Lecturas de `parent`:**  
  Lecturas directas siguiendo el estilo de `sys_getpid()` (sin locks extra para sencillez; coherente con xv6 educativo).

- **Procesos huérfanos / `init`:**  
  Los huérfanos suelen ser adoptados por `init`. Aun así, si `parent == 0` se retorna `-1`.

---

## 3. Archivos modificados y propósito

- `kernel/syscall.h`: asignación de números `SYS_getppid` y `SYS_getancestor`.  
- `kernel/syscall.c`:  
  - `extern` de `sys_getppid` y `sys_getancestor`.  
  - Mapeo en `syscalls[]`: `[SYS_getppid] sys_getppid`, `[SYS_getancestor] sys_getancestor`.  
- `kernel/sysproc.c`: implementación de:
  - `sys_getppid(void)`  
  - `sys_getancestor(void)`  
- `user/user.h`: prototipos `int getppid(void); int getancestor(int);`  
- `user/usys.pl`: entradas `entry("getppid"); entry("getancestor");`  
- `user/yosoytupadre.c`: programa de prueba.  
- `Makefile`: agregado `_yosoytupadre` en `UPROGS`.

> Commit realizado:  
> `T1: agregar syscalls getppid/getancestor y programa yosoytupadre`

---

## 4. Código relevante (extractos)

### 4.1 `kernel/sysproc.c`
```c
uint64
sys_getppid(void) {
  struct proc *p = myproc();
  if(p == 0 || p->parent == 0) return -1;
  return p->parent->pid;
}

uint64
sys_getancestor(void) {
  int n;
  argint(0, &n);            // llena 'n'
  if(n < 0) return -1;

  struct proc *q = myproc();
  while(n > 0 && q != 0) {
    q = q->parent;
    n--;
  }
  if(q == 0) return -1;
  return q->pid;
}
```

### 4.2 `user/yosoytupadre.c`
```c
int main(int argc, char *argv[]) {
  printf("yo=%d, ppid=%d\n", getpid(), getppid());
  for(int k = 0; k <= 4; k++)
    printf("ancestor(%d) = %d\n", k, getancestor(k));

  int pid = fork();
  if(pid < 0){ printf("fork fallo\n"); exit(1); }
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
```

---

## 5. Cómo compilar y ejecutar

```bash
# desde la raíz del repo
make clean
make qemu
```

En la shell de xv6:
```
yosoytupadre
```

---

## 6. Resultados (salida obtenida)

```
yo=3, ppid=2
ancestor(0) = 3
ancestor(1) = 2
ancestor(2) = 1
ancestor(3) = -1
ancestor(4) = -1
Hijo: yo=4, ppid=3
Hijo: ancestor(0) = 4
Hijo: ancestor(1) = 3
Hijo: ancestor(2) = 2
Hijo: ancestor(3) = 1
Hijo: ancestor(4) = -1
```

**Interpretación:** La cadena de ancestros es consistente: proceso → padre → `init` → `-1`.  
El hijo hereda al padre correcto (`ppid = PID del proceso padre`).

---

## 7. Casos borde y pruebas adicionales

- `getancestor(-1) → -1` (argumento inválido).  
- Profundidad grande (más allá de `init`) → `-1`.  
- Comportamiento en proceso hijo tras `fork()` (validar `ppid` y ascendencia).  
- Redirección de salida para evidencias:
  ```
  yosoytupadre > out.txt
  cat out.txt
  ```

---

## 8. Dificultades y cómo se resolvieron

- **Error de linker** `undefined reference to sys_getppid/sys_getancestor`:  
  Faltaba implementar handlers en `sysproc.c`.  
- **`unknown sys call N`**:  
  Se arregla agregando entradas en `syscalls[]` y definiendo `SYS_*` en `syscall.h`.  
- **Última barra en UPROGS**:  
  Quitar `\` del último elemento (`_yosoytupadre`).

---

## 9. Conclusiones

- Se implementaron correctamente los syscalls `getppid` y `getancestor`, y se validó su funcionamiento con un programa de usuario.  
- El comportamiento observado concuerda con el modelo de procesos y herencia de xv6.

---

## 10. Cómo reproducir (paso a paso breve)

1. `git checkout -b <nombre_apellido_t1>`  
2. Modificar/crear archivos listados en la sección 3.  
3. `make clean && make qemu`  
4. En xv6: `yosoytupadre`  
5. Confirmar salida y subir rama:  
   ```bash
   git add ...
   git commit -m "T1: ..."
   git push -u origin <rama>
   ```

---

