#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "global.h"
#include "object.h"
#include "hashtable.h"
#include "method.h"

PUBLIC OVECTOR newcompilertemplate(int argc, byte *code, int codelen, VECTOR littab) {
  OVECTOR s = newovector(ME_MAXSLOTINDEX, T_METHOD);
  BVECTOR b = newbvector_noinit(codelen);

  memcpy(b->vec, code, codelen);

  ATPUT(s, ME_NAME, (OBJ) newsym("anonymous-method"));
  ATPUT(s, ME_NEXT, NULL);
  ATPUT(s, ME_FLAGS, MKNUM(O_PERMS_MASK));
  ATPUT(s, ME_CODE, (OBJ) b);
  ATPUT(s, ME_OWNER, NULL);
  ATPUT(s, ME_LITS, (OBJ) littab);
  ATPUT(s, ME_ENV, NULL);
  ATPUT(s, ME_ARGC, MKNUM(argc));

  return s;
}

PUBLIC void addmethod(OBJECT obj, OVECTOR namesym, OBJECT owner, OVECTOR template) {
  OVECTOR s = newovector(ME_MAXSLOTINDEX, T_METHOD);

  ATPUT(s, ME_NAME, (OBJ) namesym);
  ATPUT(s, ME_NEXT, NULL);
  ATPUT(s, ME_FLAGS, MKNUM(O_PERMS_MASK | O_SETUID));
  ATPUT(s, ME_CODE, (OBJ) AT(template, ME_CODE));
  ATPUT(s, ME_OWNER, (OBJ) owner);
  ATPUT(s, ME_LITS, (OBJ) AT(template, ME_LITS));
  ATPUT(s, ME_ENV, (OBJ) AT(template, ME_ENV));
  ATPUT(s, ME_ARGC, (OBJ) AT(template, ME_ARGC));

  hashtable_put(obj->methods, namesym, s);
}

PUBLIC OVECTOR findmethod(OBJECT obj, OVECTOR namesym) {
  OVECTOR s;
  int i;

  while (1) {
    OBJ objparents;

    s = hashtable_get(obj->methods, namesym);
    objparents = obj->parents;

    if (s != NULL)
      return s;

    if (objparents == NULL)
      return NULL;

    if (OBJECTP(objparents)) {
      obj = (OBJECT) objparents;
      continue;
    }

    for (i = 0; i < objparents->length; i++) {
      s = findmethod((OBJECT) AT((VECTOR) objparents, i), namesym);
      if (s != NULL)
	return s;
    }
  }

  return NULL;
}
