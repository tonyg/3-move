#include "global.h"
#include "object.h"
#include "vm.h"
#include "prim.h"
#include "scanner.h"
#include "parser.h"
#include "conn.h"
#include "perms.h"
#include "thread.h"
#include "PRIM.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if 0
DEFPRIM(getInConn) {
  return (OBJ) vms->r->vm_input;
}
#endif

#if 0
DEFPRIM(getOutConn) {
  return (OBJ) vms->r->vm_output;
}
#endif

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

DEFPRIM(setEffuid) {
  OBJ u = ARG(0);
  TYPEERRIF(!OBJECTP(u));

  if (!PRIVILEGEDP(vms->r->vm_effuid))
    return false;

  if (vms->r->vm_frame == NULL)
    return false;

  ATPUT(vms->r->vm_frame, FR_EFFUID, (OBJ) u);
  return true;
}

DEFPRIM(setRealuid) {
  OBJ u = ARG(0);
  TYPEERRIF(!OBJECTP(u));

  if (!PRIVILEGEDP(vms->r->vm_effuid))
    return false;

  vms->r->vm_uid = (OBJECT) u;
  return true;
}

DEFPRIM(defineGlobal) {
  OBJ name = ARG(0);
  OBJ value = ARG(1);

  TYPEERRIF(!OVECTORP(name) || ((OVECTOR) name)->type != T_SYMBOL);

  if (!PRIVILEGEDP(vms->r->vm_effuid))
    return false;

  ATPUT((OVECTOR) name, SY_VALUE, value);
  return true;
}

DEFPRIM(typeOfFun) {
  OBJ x = ARG(0);

  if (x == NULL)
    return (OBJ) newsym("null");

  if (NUMP(x))
    return (OBJ) newsym("number");

  if (SINGLETONP(x)) {
    if (x == true || x == false) return (OBJ) newsym("boolean");
    return (OBJ) newsym("singleton");
  }

  switch (x->kind) {
    case KIND_OBJECT: return (OBJ) newsym("object");
    case KIND_BVECTOR: return (OBJ) newsym("string");
    case KIND_OVECTOR:
      switch (((OVECTOR) x)->type) {
	case T_HASHTABLE: return (OBJ) newsym("hashtable");
	case T_SLOT: return (OBJ) newsym("slot");
	case T_METHOD: return (OBJ) newsym("method");
	case T_CLOSURE: return (OBJ) newsym("closure");
	case T_SYMBOL: return (OBJ) newsym("symbol");
	case T_PRIM: return (OBJ) newsym("primitive");
	case T_FRAME: return (OBJ) newsym("frame");
	case T_VMREGS: return (OBJ) newsym("vm-register-set");
	case T_CONNECTION: return (OBJ) newsym("connection");
	case T_CONTINUATION: return (OBJ) newsym("continuation");
	case T_USERHASHLINK: return (OBJ) newsym("hashlink");
	default: return (OBJ) newsym("unknown-ovector-type");
      }
    case KIND_VECTOR: return (OBJ) newsym("vector");
    default: return (OBJ) newsym("unknown-type");
  }
}

DEFPRIM(setHandlerFun) {
  OBJ h = ARG(0);

  TYPEERRIF(h != NULL && (!OVECTORP(h) || (((OVECTOR) h)->type != T_CLOSURE &&
					   ((OVECTOR) h)->type != T_PRIM)));

  if (!PRIVILEGEDP(vms->r->vm_effuid))
    return false;

  vms->r->vm_trap_closure = (OVECTOR) h;
  return true;
}

#define ISACONN(x)	(OVECTORP(x) && ((OVECTOR) x)->type == T_CONNECTION)

DEFPRIM(setConnectionsFun) {
  OBJ i = ARG(0);
  OBJ o = ARG(1);

  TYPEERRIF(!ISACONN(i) || !ISACONN(o));

  if (!PRIVILEGEDP(vms->r->vm_effuid))
    return false;

  vms->r->vm_input = (OVECTOR) i;
  vms->r->vm_output = (OVECTOR) o;
  return true;
}

DEFPRIM(getCurrThreadFun) {
  THREAD t = find_thread_by_tid(pthread_self());
  if (t == NULL)
    return false;
  return MKNUM(t->number);
}

DEFPRIM(setThreadQuotaFun) {
  OBJ tnum = ARG(0);
  OBJ q = ARG(1);
  THREAD t;

  TYPEERRIF(!NUMP(tnum) || !NUMP(q));

  if (!PRIVILEGEDP(vms->r->vm_effuid))
    return false;

  t = find_thread_by_number(NUM(tnum));
  t->vms->c.vm_state = NUM(q);

  return true;
}

DEFPRIM(checkpointFun) {
  if (!PRIVILEGEDP(vms->r->vm_effuid))
    return false;

  synch_bitmap |= SYNCH_CHECKPOINT;
  return true;
}

DEFPRIM(callCCFun) {
  OBJ c = ARG(0);
  OVECTOR closure = (OVECTOR) c;
  VECTOR av;

  TYPEERRIF(!(OVECTORP(c) && closure->type == T_CLOSURE));

  av = newvector_noinit(2);
  ATPUT(av, 0, NULL);
  ATPUT(av, 1, (OBJ) getcont_from(vms));

  apply_closure(vms, closure, av);
  push_frame(vms);

  return vms->r->vm_acc;
}

DEFPRIM(setuidPFun) {
  OBJ c = ARG(0);
  OVECTOR clos = (OVECTOR) c;
  OVECTOR meth;

  TYPEERRIF(!(OVECTORP(c) && clos->type == T_CLOSURE));

  meth = (OVECTOR) AT(clos, CL_METHOD);

  return NUM(AT(meth, ME_FLAGS)) & O_SETUID ? true : false;
}

DEFPRIM(setSetuidFun) {
  OBJ c = ARG(0);
  OVECTOR clos = (OVECTOR) c;
  OBJ newval = ARG(1);
  OVECTOR meth;
  u32 fl;

  TYPEERRIF(!(OVECTORP(c) && clos->type == T_CLOSURE));

  if (!PRIVILEGEDP(vms->r->vm_effuid))
    return false;

  meth = (OVECTOR) AT(clos, CL_METHOD);
  fl = NUM(AT(meth, ME_FLAGS));
  
  if (newval == false)
    fl &= ~O_SETUID;
  else
    fl |= O_SETUID;

  ATPUT(meth, ME_FLAGS, MKNUM(fl));
  return true;
}

DEFPRIM(callerEffuidFun) {
  OVECTOR f = vms->r->vm_frame;

  if (f == NULL)
    return undefined;

  f = (OVECTOR) AT(f, FR_FRAME);

  if (f == NULL)
    return undefined;

  return AT(f, FR_EFFUID);
}

DEFPRIM(inGroupOfFun) {
  OBJ w = ARG(0);
  OBJECT what = (OBJECT) w;
  OBJ o = ARG(1);
  OBJECT obj = (OBJECT) o;

  TYPEERRIF(!OBJECTP(w) || !OBJECTP(o));

  return in_group(what, obj->group) ? true : false;
}

DEFPRIM(shutdownFun) {
  if (!PRIVILEGEDP(vms->r->vm_effuid))
    return false;

  exit(0);
}

PUBLIC void install_PRIM_system(void) {
  /* register_prim("input-conn", 0x02001, getInConn); */
  /* register_prim("output-conn", 0x02002, getOutConn); */
  register_prim("compile", 0x02003, compileString);
  register_prim("effuid", 0x02004, getEffuid);
  register_prim("realuid", 0x02005, getRealuid);
  register_prim("set-effuid", 0x02006, setEffuid);
  register_prim("set-realuid", 0x02007, setRealuid);
  register_prim("define-global", 0x02008, defineGlobal);
  register_prim("type-of", 0x02009, typeOfFun);
  register_prim("set-exception-handler", 0x0200A, setHandlerFun);
  register_prim("set-connections", 0x0200B, setConnectionsFun);
  register_prim("current-thread", 0x0200C, getCurrThreadFun);
  register_prim("set-thread-quota", 0x0200D, setThreadQuotaFun);
  register_prim("checkpoint", 0x0200E, checkpointFun);
  register_prim("call/cc", 0x0200F, callCCFun);
  register_prim("setuid?", 0x02010, setuidPFun);
  register_prim("set-setuid", 0x02011, setSetuidFun);
  register_prim("caller-effuid", 0x02012, callerEffuidFun);
  register_prim("in-group-of", 0x02013, inGroupOfFun);
  register_prim("shutdown", 0x02014, shutdownFun);
}
