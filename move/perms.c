#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "global.h"
#include "object.h"
#include "perms.h"

PUBLIC int in_group(OBJECT what, VECTOR group) {
  int i;

  if (group == NULL)
    return 0;

  for (i = 0; i < group->_.length; i++) {
    OBJ x = AT(group, i);

    if (x == (OBJ) what)
      return 1;

    if (VECTORP(x))
      if (in_group(what, (VECTOR) x))
	return 1;
  }

  return 0;
}
