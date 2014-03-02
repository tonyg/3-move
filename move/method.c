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
#include <stdio.h>
#include <string.h>

#include "global.h"
#include "object.h"
#include "hashtable.h"
#include "method.h"

PUBLIC OVECTOR newcompilertemplate(int argc, uint8_t *code, int codelen, VECTOR littab) {
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
