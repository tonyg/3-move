#include <stdlib.h>

#include "global.h"
#include "object.h"
#include "gc.h"
#include "vm.h"
#include "thread.h"
#include "prim.h"

#if 0
#define DEBUG
#endif

#define PRIMTAB_SIZE	419

typedef struct primrec {
  char *name;
  int primargc;
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

PUBLIC void register_prim(int primargc, char *name, int number, prim_fn fn) {
  int hash = number % PRIMTAB_SIZE;
  primrec *p = allocmem(sizeof(primrec));

  p->name = name;
  p->primargc = primargc;
  p->number = number;
  p->fn = fn;
  p->next = primtab[hash];
  primtab[hash] = p;
}

PUBLIC void bind_primitives_to_symbols(void) {
  int i;

  for (i = 0; i < PRIMTAB_SIZE; i++) {
    primrec *p = primtab[i];

    while (p != NULL) {
      OVECTOR pname = newsym(p->name);
      if (AT(pname, SY_VALUE) == undefined) {
	OVECTOR pobj = newovector(PR_MAXSLOTINDEX, T_PRIM);
	ATPUT(pobj, PR_NAME, (OBJ) pname);
	ATPUT(pobj, PR_NUMBER, MKNUM(p->number));
	ATPUT(pname, SY_VALUE, (OBJ) pobj);
      }

      p = p->next;
    }
  }
}

PUBLIC prim_fn lookup_prim(int number, int *primargc) {
  int hash = number % PRIMTAB_SIZE;
  primrec *p = primtab[hash];

  while (p != NULL) {
    if (p->number == number) {
#ifdef DEBUG
      printf("%p looked up prim %s\n", current_thread, p->name);
      fflush(stdout);
#endif
      if (primargc != NULL)
	*primargc = p->primargc;
      return p->fn;
    }

    p = p->next;
  }

  return NULL;
}
