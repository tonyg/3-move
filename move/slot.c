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
#include "slot.h"

PUBLIC OVECTOR addslot(OBJECT obj, OVECTOR namesym, OBJECT owner) {
  OVECTOR s = newovector(SL_MAXSLOTINDEX, T_SLOT);

  ATPUT(s, SL_NAME, (OBJ) namesym);
  ATPUT(s, SL_NEXT, NULL);
  ATPUT(s, SL_FLAGS, MKNUM(O_OWNER_MASK));
  ATPUT(s, SL_VALUE, undefined);
  ATPUT(s, SL_OWNER, (OBJ) owner);

  hashtable_put(obj->attributes, namesym, s);

  return s;
}

PUBLIC OVECTOR findslot(OBJECT obj, OVECTOR namesym, OBJECT *foundin) {
  OVECTOR s;
  int i;

  while (1) {
    OBJ objparents;

    s = hashtable_get(obj->attributes, namesym);
    objparents = obj->parents;

    if (s != NULL) {
      if (foundin != NULL)
	*foundin = obj;
      return s;
    }

    if (objparents == NULL)
      return NULL;

    if (OBJECTP(objparents)) {
      obj = (OBJECT) objparents;
      continue;
    }

    for (i = 0; i < objparents->length; i++) {
      s = findslot((OBJECT) AT((VECTOR) objparents, i), namesym, foundin);
      if (s != NULL)
	return s;
    }

    break;
  }

  return NULL;
}

PUBLIC void delslot(OBJECT obj, OVECTOR namesym) {
  hashtable_remove(obj->attributes, namesym);
}
