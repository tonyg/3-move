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

  LOCK(obj);
  hashtable_put(obj->attributes, namesym, s);
  UNLOCK(obj);

  return s;
}

PUBLIC OVECTOR findslot(OBJECT obj, OVECTOR namesym, OBJECT *foundin) {
  OVECTOR s;
  int i;

  while (1) {
    OBJ objparents;

    LOCK(obj);
    s = hashtable_get(obj->attributes, namesym);
    objparents = obj->parents;
    UNLOCK(obj);

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
