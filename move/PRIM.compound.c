#include "global.h"
#include "object.h"
#include "vm.h"
#include "prim.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define DEFPRIM(name)	PRIVATE OBJ name(VMSTATE vms, VECTOR argvec)
#define ARG(n)		AT(argvec, (n)+1)

DEFPRIM(vectorRef) {
  OBJ x = ARG(0);
  OBJ i = ARG(1);

  if ((!VECTORP(x) && !BVECTORP(x)) || !NUMP(i)) {
    vm_raise(vms, (OBJ) newsym("type-error"), (OBJ) argvec);
    return undefined;
  }

  if (NUM(i) < 0 || NUM(i) >= x->length) {
    vm_raise(vms, (OBJ) newsym("range-error"), (OBJ) argvec);
    return undefined;
  }

  if (VECTORP(x)) {
    return AT((VECTOR) x, NUM(i));
  } else {
    BVECTOR res = newbvector_noinit(1);

    ATPUT(res, 0, AT((BVECTOR) x, NUM(i)));
    return (OBJ) res;
  }
}

DEFPRIM(vectorSet) {
  OBJ x = ARG(0);
  OBJ i = ARG(1);
  OBJ v = ARG(2);

  if ((!VECTORP(x) && !BVECTORP(x)) || !NUMP(i)) {
    vm_raise(vms, (OBJ) newsym("type-error"), (OBJ) argvec);
    return undefined;
  }

  if (NUM(i) < 0 || NUM(i) >= x->length) {
    vm_raise(vms, (OBJ) newsym("range-error"), (OBJ) argvec);
    return undefined;
  }

  if (VECTORP(x))
    ATPUT((VECTOR) x, NUM(i), v);
  else {
    if (!BVECTORP(v) || v->length < 1) {
      vm_raise(vms, (OBJ) newsym("type-error"), (OBJ) argvec);
      return undefined;
    }

    ATPUT((BVECTOR) x, NUM(i), AT((BVECTOR) v, 0));
  }

  return v;
}

PUBLIC void installer(void) {
  register_prim("vector-ref", 0x01001, vectorRef);
  register_prim("vector-set", 0x01002, vectorSet);
}
