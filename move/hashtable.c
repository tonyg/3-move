#include "global.h"
#include "object.h"
#include "hashtable.h"

PUBLIC OVECTOR newhashtable(int len) {
  OVECTOR ht = newovector(len + 1, T_HASHTABLE);

  ATPUT(ht, 0, MKNUM(0));

  return ht;
}

PUBLIC OVECTOR hashtable_get(OVECTOR table, OVECTOR keysym) {
  u32 h = (NUM(AT(keysym, SY_HASH)) % (table->_.length - 1)) + 1;
  OVECTOR value = (OVECTOR) AT(table, h);
  
  while (value != NULL) {
    if ((OVECTOR) AT(value, HASHLINK_NAME) == keysym)
      return value;

    value = (OVECTOR) AT(value, HASHLINK_NEXT);
  }

  return NULL;
}

PUBLIC void hashtable_put(OVECTOR table, OVECTOR keysym, OVECTOR value) {
  u32 h = (NUM(AT(keysym, SY_HASH)) % (table->_.length - 1)) + 1;
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
}