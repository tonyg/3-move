#include "global.h"
#include "object.h"
#include "vm.h"
#include "prim.h"
#include "scanner.h"
#include "parser.h"
#include "conn.h"
#include "perms.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define TYPEERRIF(c)	if (c) { \
			  vm_raise(vms, (OBJ) newsym("type-error"), (OBJ) argvec); \
			  return undefined; \
			}
#define DEFPRIM(name)	PRIVATE OBJ name(VMSTATE vms, VECTOR argvec)
#define ARG(n)		AT(argvec, (n)+1)

DEFPRIM(getInConn) {
  return (OBJ) vms->r->vm_input;
}

DEFPRIM(getOutConn) {
  return (OBJ) vms->r->vm_output;
}

DEFPRIM(compileString) {
  OBJ s = ARG(0);
  BVECTOR str = (BVECTOR) s;
  OVECTOR method;
  ScanInst si;
  OVECTOR closure;

  TYPEERRIF(!BVECTORP(s));

  fill_scaninst(&si, newstringconn(str));
  method = parse(vms, &si);

  if (method == NULL)
    return false;

  ATPUT(method, ME_OWNER, (OBJ) vms->r->vm_effuid);

  closure = newovector(CL_MAXSLOTINDEX, T_CLOSURE);
  ATPUT(closure, CL_METHOD, (OBJ) method);
  ATPUT(closure, CL_SELF, NULL);

  return (OBJ) closure;
}

DEFPRIM(getEffuid) {
  return (OBJ) vms->r->vm_effuid;
}

DEFPRIM(getRealuid) {
  return (OBJ) vms->r->vm_uid;
}

DEFPRIM(defineGlobal) {
  OBJ name = ARG(0);
  OBJ value = ARG(1);

  TYPEERRIF(!OVECTORP(name) || ((OVECTOR) name)->type != T_SYMBOL);

  if (vms->r->vm_effuid != NULL && !PRIVILEGEDP(vms->r->vm_effuid))
    return false;

  ATPUT((OVECTOR) name, SY_VALUE, value);
  return true;
}

PUBLIC void installer(void) {
  register_prim("input-conn", 0x02001, getInConn);
  register_prim("output-conn", 0x02002, getOutConn);
  register_prim("compile", 0x02003, compileString);
  register_prim("effuid", 0x02004, getEffuid);
  register_prim("realuid", 0x02005, getRealuid);
  register_prim("define-global", 0x02006, defineGlobal);
}
