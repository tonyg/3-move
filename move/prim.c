#include <stdlib.h>

#include "global.h"
#include "object.h"
#include "gc.h"
#include "vm.h"
#include "prim.h"

#define PRIMTAB_SIZE	419

typedef struct primrec {
  char *name;
  int number;
  prim_fn fn;
  struct primrec *next;
} primrec;

PRIVATE primrec *primtab[PRIMTAB_SIZE];

PUBLIC void init_prim(void) {
  int i;

  for (i = 0; i < PRIMTAB_SIZE; i++)
    primtab[i] = NULL;
}

PUBLIC void register_prim(char *name, int number, prim_fn fn) {
  int hash = number % PRIMTAB_SIZE;
  primrec *p = allocmem(sizeof(primrec));
  OVECTOR pobj;
  OVECTOR pname;

  p->name = name;
  p->number = number;
  p->fn = fn;
  p->next = primtab[hash];
  primtab[hash] = p;

  pobj = newovector(PR_MAXSLOTINDEX, T_PRIM);
  pname = newsym(name);
  ATPUT(pobj, PR_NAME, (OBJ) pname);
  ATPUT(pobj, PR_NUMBER, MKNUM(number));
  ATPUT(pname, SY_VALUE, (OBJ) pobj);
}

PUBLIC prim_fn lookup_prim(int number) {
  int hash = number % PRIMTAB_SIZE;
  primrec *p = primtab[hash];

  while (p != NULL) {
    if (p->number == number)
      return p->fn;

    p = p->next;
  }

  return NULL;
}
