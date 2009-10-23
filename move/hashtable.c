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

#include "global.h"
#include "object.h"
#include "hashtable.h"

PUBLIC OVECTOR newhashtable(int len) {
  OVECTOR ht = newovector(len + 1, T_HASHTABLE);

  ATPUT(ht, 0, MKNUM(0));

  return ht;
}

PUBLIC OVECTOR hashtable_get(OVECTOR table, OVECTOR keysym) {
  unum h = (NUM(AT(keysym, SY_HASH)) % (table->_.length - 1)) + 1;
  OVECTOR value = (OVECTOR) AT(table, h);
  
  while (value != NULL) {
    if ((OVECTOR) AT(value, HASHLINK_NAME) == keysym)
      return value;

    value = (OVECTOR) AT(value, HASHLINK_NEXT);
  }

  return NULL;
}

PUBLIC void hashtable_put(OVECTOR table, OVECTOR keysym, OVECTOR value) {
  unum h = (NUM(AT(keysym, SY_HASH)) % (table->_.length - 1)) + 1;
  OVECTOR curr, prev;

  curr = (OVECTOR) AT(table, h);
  prev = NULL;

  while (curr != NULL) {
    if ((OVECTOR) AT(curr, HASHLINK_NAME) == keysym) {
      ATPUT(value, HASHLINK_NEXT, AT(curr, HASHLINK_NEXT));
      if (prev == NULL)
	ATPUT(table, h, (OBJ) value);
      else
	ATPUT(prev, HASHLINK_NEXT, (OBJ) value);

      return;
    }

    prev = curr;
    curr = (OVECTOR) AT(curr, HASHLINK_NEXT);
  }

  ATPUT(value, HASHLINK_NEXT, AT(table, h));
  ATPUT(table, h, (OBJ) value);
  ATPUT(table, 0, MKNUM(NUM(AT(table, 0)) + 1));
}

PUBLIC void hashtable_remove(OVECTOR table, OVECTOR keysym) {
  unum h = (NUM(AT(keysym, SY_HASH)) % (table->_.length - 1)) + 1;
  OVECTOR curr, prev;

  curr = (OVECTOR) AT(table, h);
  prev = NULL;

  while (curr != NULL) {
    if ((OVECTOR) AT(curr, HASHLINK_NAME) == keysym) {
      if (prev == NULL)
	ATPUT(table, h, (OBJ) AT(curr, HASHLINK_NEXT));
      else
	ATPUT(prev, HASHLINK_NEXT, (OBJ) AT(curr, HASHLINK_NEXT));
      ATPUT(curr, HASHLINK_NEXT, NULL);

      return;
    }

    prev = curr;
    curr = (OVECTOR) AT(curr, HASHLINK_NEXT);
  }
}

PUBLIC VECTOR enumerate_keys(OVECTOR table) {
  VECTOR result = newvector_noinit(NUM(AT(table, 0)));
  int i;
  int j = 0;

  for (i = 1; i < table->_.length; i++) {
    OVECTOR link = (OVECTOR) AT(table, i);

    while (link != NULL) {
      ATPUT(result, j, AT(link, HASHLINK_NAME));
      j++;
      link = (OVECTOR) AT(link, HASHLINK_NEXT);
    }
  }

  return result;
}
