#include "global.h"
#include "object.h"
#include "vm.h"
#include "prim.h"
#include "gc.h"
#include "thread.h"
#include "perms.h"
#include "PRIM.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

DEFPRIM(randomFun) {
  OBJ n = ARG(0);
  TYPEERRIF(!NUMP(n));
  return MKNUM(random() % NUM(n));
}

DEFPRIM(absFun) {
  OBJ n = ARG(0);
  TYPEERRIF(!NUMP(n));
  return MKNUM(abs(NUM(n)));
}

DEFPRIM(timeFun) {
  return MKNUM(time(NULL));
}

DEFPRIM(timeStringFun) {
  OBJ n = ARG(0);
  time_t val;
  char *s;

  TYPEERRIF(!NUMP(n));

  val = UNUM(n);
  s = ctime(&val);
  return (OBJ) newstring(s);
}

DEFPRIM(timeVectorFun) {
  OBJ n = ARG(0);
  time_t val;
  struct tm s;
  VECTOR result;

  TYPEERRIF(!NUMP(n));

  val = UNUM(n);
  s = *localtime(&val);
  result = newvector_noinit(8);
  ATPUT(result, 0, MKNUM(s.tm_year + 1900));	/* This is correct behaviour wrt y2k according
						   to the manpage for localtime/struct tm */

  ATPUT(result, 1, MKNUM(s.tm_mon + 1));	/* so that it's 1-based, not 0-based. */
  ATPUT(result, 2, MKNUM(s.tm_mday));
  ATPUT(result, 3, MKNUM(s.tm_hour));
  ATPUT(result, 4, MKNUM(s.tm_min));
  ATPUT(result, 5, MKNUM(s.tm_sec));
  ATPUT(result, 6, MKNUM(s.tm_wday));
  ATPUT(result, 7, MKNUM(s.tm_yday));
  return (OBJ) result;
}

DEFPRIM(forkFun) {
  OBJ c = ARG(0);
  TYPEERRIF(!(OVECTORP(c) && ((OVECTOR) c)->type == T_CLOSURE));
  return MKNUM(begin_thread((OVECTOR) c, vms)->number);
}

DEFPRIM(killFun) {
  OBJ tnum = ARG(0);
  OBJ excp = ARG(1);
  OBJ arg = ARG(2);
  THREAD t;

  TYPEERRIF(!NUMP(tnum));

  if (!PRIVILEGEDP(vms->r->vm_effuid))
    return false;

  t = find_thread_by_number(NUM(tnum));
  vm_raise(t->vms, excp, arg);

  return true;
}

DEFPRIM(sleepFun) {
  OBJ len = ARG(0);
  int numseconds = NUM(len);
  
  TYPEERRIF(!NUMP(len));

  if (numseconds > 600 && !PRIVILEGEDP(vms->r->vm_effuid))
    return false;

  gc_dec_safepoints();
  sleep(numseconds);
  gc_inc_safepoints();

  return true;
}

DEFPRIM(equalPFun) {
  OBJ a = ARG(0);
  OBJ b = ARG(1);

  if (BVECTORP(a) && BVECTORP(b))
    return (a->length == b->length &&
	    !strncasecmp(((BVECTOR) a)->vec, ((BVECTOR) b)->vec, a->length)) ? true : false;

  return a == b ? true : false;
}

DEFPRIM(asNumFun) {
  OBJ x = ARG(0);

  if (NUMP(x))
    return x;

  if (BVECTORP(x)) {
    char buf[1024];
    memcpy(buf, ((BVECTOR) x)->vec, x->length);
    buf[x->length] = '\0';
    return MKNUM(atol(buf));
  }

  return undefined;
}

PUBLIC void install_PRIM_misc(void) {
  srandom(time(NULL));

  register_prim("random", 0x03001, randomFun);
  register_prim("abs", 0x03002, absFun);
  register_prim("time", 0x03003, timeFun);
  register_prim("time-string", 0x03004, timeStringFun);
  register_prim("time-vector", 0x03005, timeVectorFun);
  register_prim("fork", 0x03006, forkFun);
  register_prim("kill", 0x03007, killFun);
  register_prim("sleep", 0x03008, sleepFun);
  register_prim("equal?", 0x03009, equalPFun);
  register_prim("as-num", 0x0300A, asNumFun);
}
