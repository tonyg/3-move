/*
 * 3-MOVE, a multi-user networked online text-based programmable virtual environment
 * Copyright 1997, 1998, 1999, 2003, 2005, 2008, 2009 Tony Garnock-Jones <tonyg@kcbbs.gen.nz>
 *
 * 3-MOVE is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * 3-MOVE is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with 3-MOVE.  If not, see <http://www.gnu.org/licenses/>.
 */

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
      printf("%d looked up prim %s\n", current_thread ? current_thread->number : -1, p->name);
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
