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
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "global.h"
#include "object.h"
#include "gc.h"
#include "scanner.h"
#include "conn.h"

#include "pair.h"
#include "hashtable.h"

#define SYMTAB_SIZE	2131	/* prime */

PUBLIC VECTOR symtab;

PRIVATE Obj zero_vector;

PUBLIC void init_object(void) {
  symtab = newvector(SYMTAB_SIZE);

  /* protect((OBJ *) &symtab); NOT NECESSARY NOW THAT SYMBOL TABLE IS SPECIALLY SWEPT */

  zero_vector.next = NULL;
  zero_vector.kind = KIND_VECTOR;
  zero_vector.marked = 0;
  zero_vector.length = 0;
}

PUBLIC OBJECT newobject(OBJ parents, OBJECT owner) {
  OBJECT o = (OBJECT) getmem(sizeof(Object), KIND_OBJECT, 0);

  o->finalize = 0;
  o->flags = O_OWNER_MASK;

  o->methods = newhashtable(19);
  o->attributes = newhashtable(19);		/* Overrides no methods or attributes */
						/* Gak, that reminds me, hashtabs should grow! */

  o->parents = parents;
  o->owner = owner;
  o->location = o->owner;
  o->contents = NULL;

  if (o->owner != NULL) {
    VECTOR link;

    CONS(link, (OBJ) o, (OBJ) o->owner->contents);
    o->owner->contents = link;

    o->group = o->owner->group;
  } else
    o->group = NULL;

  recmutex_init(&(o->lock));

  return o;
}

PUBLIC VECTOR newvector(int len) {
  VECTOR v = newvector_noinit(len);
  int i;

  for (i = 0; i < len; i++)
    ATPUT(v, i, NULL);

  return v;
}

PUBLIC VECTOR newvector_noinit(int len) {
  if (len > 0) {
    VECTOR v = (VECTOR) getmem(sizeof(Vector) + len * sizeof(OBJ), KIND_VECTOR, len);
    return v;
  } else
    return (VECTOR) &zero_vector;
}

PUBLIC VECTOR vector_clone(VECTOR v) {
  VECTOR result = newvector_noinit(v->_.length);
  int i;

  for (i = 0; i < v->_.length; i++)
    ATPUT(result, i, AT(v, i));

  return result;
}

PUBLIC BVECTOR newbvector_noinit(int len) {
  BVECTOR bv = (BVECTOR) getmem(sizeof(BVector) + len * sizeof(uint8_t), KIND_BVECTOR, len);

  return bv;
}

PUBLIC BVECTOR newbvector(int len) {
  BVECTOR bv = newbvector_noinit(len);
  memset(bv->vec, 0, len);
  return bv;
}

PUBLIC BVECTOR newstring(char *buf) {
  int len = strlen(buf);
  BVECTOR bv = newbvector_noinit(len);

  memcpy(bv->vec, buf, len);

  return bv;
}

PUBLIC OVECTOR newovector(int len, int type) {
  OVECTOR ov = newovector_noinit(len, type);
  int i;

  for (i = 0; i < len; i++)
    ATPUT(ov, i, NULL);

  return ov;
}

PUBLIC OVECTOR newovector_noinit(int len, int type) {
  OVECTOR ov = (OVECTOR) getmem(sizeof(OVector) + len * sizeof(OBJ), KIND_OVECTOR, len);

  ov->finalize = 0;
  ov->type = type;

  return ov;
}

PUBLIC void finalize_ovector(OVECTOR o) {
  switch (o->type) {
    case T_CONNECTION:
      conn_close(o);
      break;

    default:
      break;
  }
}

/* An implementation of hashpjw, originally written by P.J. Weinberger
   in a C compiler. Taken from "Compilers: Principles, Techniques and
   Tools" by Aho, Sethi and Ullman
                (Addison-Wesley 1986, ISBN 0-201-10194-7) */

PUBLIC unsigned long hash_str(char *string) {
    unsigned long hash = 0, g;

    if (string == NULL) return 0;

    while (*string) {
        hash = (hash << 4) + tolower(*string++);
        g = hash & 0xf0000000L;
        if (g) {
            hash = hash ^ (g >> 24);
            hash = hash ^ g;
        }
    }
    return hash;
}

PUBLIC OVECTOR newsym(char *name) {
  unum rawhash = UNUM(MKNUM(hash_str(name)));
  unum h = rawhash % SYMTAB_SIZE;
  OVECTOR sym;

  sym = (OVECTOR) AT(symtab, h);

  while (sym != NULL) {
    BVECTOR symname = (BVECTOR) AT(sym, SY_NAME);

    if (!strncmp((char *) symname->vec, name, symname->_.length)) {
      return sym;
    }

    sym = (OVECTOR) AT(sym, SY_NEXT);
  }

  sym = newovector_noinit(SY_MAXSLOTINDEX, T_SYMBOL);

  ATPUT(sym, SY_NAME, (OBJ) newstring(name));
  ATPUT(sym, SY_NEXT, AT(symtab, h));
  ATPUT(sym, SY_VALUE, undefined);
  ATPUT(sym, SY_HASH, MKNUM(rawhash));

  ATPUT(symtab, h, (OBJ) sym);

  return sym;
}
