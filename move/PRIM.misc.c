#include "global.h"
#include "object.h"
#include "vm.h"
#include "prim.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define DEFPRIM(name)	PRIVATE OBJ name(VMSTATE vms, VECTOR argvec)
#define ARG(n)		AT(argvec, (n)+1)

PUBLIC void installer(void) {
  /* 0x03 */
}
