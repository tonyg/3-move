#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "global.h"
#include "object.h"
#include "barrier.h"
#include "gc.h"
#include "scanner.h"
#include "conn.h"

#include "pair.h"
#include "hashtable.h"

#define SYMTAB_SIZE	419	/* prime */

PRIVATE pthread_mutex_t symtab_mutex;
PRIVATE VECTOR symtab;

PUBLIC void init_object(void) {
  pthread_mutex_init(&symtab_mutex, NULL);
  symtab = newvector(SYMTAB_SIZE);
  protect((OBJ *) &symtab);
}

PUBLIC OBJECT newobject(OBJ parents, OBJECT owner) {
  OBJECT o = (OBJECT) getmem(sizeof(Object), KIND_OBJECT, 0);

  o->finalize = 0;
  o->flags = O_OWNER_MASK&~O_ALL_C;

  o->methods = NULL;
  o->attributes = NULL;		/* Overrides no methods or attributes */

  o->parents = parents;
  o->owner = owner;
  o->location = o->owner;

  if (o->owner != NULL) {
    VECTOR link;

    LOCK(o->owner);
    CONS(link, (OBJ) o, (OBJ) o->owner->contents);
    o->owner->contents = link;
    UNLOCK(o->owner);

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
  VECTOR v = (VECTOR) getmem(sizeof(Vector) + len * sizeof(OBJ), KIND_VECTOR, len);

  return v;
}

PUBLIC BVECTOR newbvector_noinit(int len) {
  BVECTOR bv = (BVECTOR) getmem(sizeof(BVector) + len * sizeof(byte), KIND_BVECTOR, len);

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
  u32 rawhash = hash_str(name);
  u32 h = rawhash % SYMTAB_SIZE;
  OVECTOR sym;

  pthread_mutex_lock(&symtab_mutex);

  sym = (OVECTOR) AT(symtab, h);

  while (sym != NULL) {
    BVECTOR symname = (BVECTOR) AT(sym, SY_NAME);

    if (!strncmp(symname->vec, name, symname->_.length)) {
      pthread_mutex_unlock(&symtab_mutex);
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

  pthread_mutex_unlock(&symtab_mutex);
  return sym;
}
