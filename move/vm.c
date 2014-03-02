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
#include "method.h"
#include "gc.h"
#include "vm.h"
#include "bytecode.h"
#include "slot.h"
#include "prim.h"
#include "perms.h"
#include "persist.h"
#include "thread.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#if 0
#define DEBUG
#endif

/************************************************************************/
/* Private parts							*/

#define VMSTACKLENGTH	1024

#define CODEAT(i)	AT(vms->r->vm_code, i)
#define CODE16AT(i)	(((uint16_t) CODEAT(i) << 8) | (uint16_t) CODEAT((i)+1))

#define PUSH(o)		push(vms, o)
#define POP()		pop(vms)
#define PEEK()		peek(vms)

PRIVATE INLINE void push(VMSTATE vms, OBJ o) {
  if (vms->c.vm_top >= VMSTACKLENGTH) {
    vms->c.vm_top = 0;
    vms->c.vm_state = VM_STATE_DYING;
  }

  ATPUT(vms->r->vm_stack, vms->c.vm_top, o);
  vms->c.vm_top++;
}

PRIVATE INLINE OBJ pop(VMSTATE vms) {
  if (vms->c.vm_top <= 0){
    vms->c.vm_top = 1;
    vms->c.vm_state = VM_STATE_DYING;
  }

  vms->c.vm_top--;
  return AT(vms->r->vm_stack, vms->c.vm_top);
}

PRIVATE INLINE OBJ peek(VMSTATE vms) {
  return AT(vms->r->vm_stack, vms->c.vm_top - 1);
}

PRIVATE INLINE void fillframe(VMSTATE vms, OVECTOR f, uint32_t newip) {
  ATPUT(f, FR_CODE, (OBJ) vms->r->vm_code);
  ATPUT(f, FR_IP, MKNUM(newip));
  ATPUT(f, FR_SELF, (OBJ) vms->r->vm_self);
  ATPUT(f, FR_LITS, (OBJ) vms->r->vm_lits);
  ATPUT(f, FR_ENV, (OBJ) vms->r->vm_env);
  ATPUT(f, FR_FRAME, (OBJ) vms->r->vm_frame);
  ATPUT(f, FR_METHOD, (OBJ) vms->r->vm_method);
  ATPUT(f, FR_EFFUID, (OBJ) vms->r->vm_effuid);
}

PRIVATE INLINE void restoreframe(VMSTATE vms, OVECTOR f) {
  vms->r->vm_code = (BVECTOR) AT(f, FR_CODE);
  vms->c.vm_ip = NUM(AT(f, FR_IP));
  vms->r->vm_self = (OBJECT) AT(f, FR_SELF);
  vms->r->vm_lits = (VECTOR) AT(f, FR_LITS);
  vms->r->vm_env = (VECTOR) AT(f, FR_ENV);
  vms->r->vm_frame = (OVECTOR) AT(f, FR_FRAME);
  vms->r->vm_method = (OVECTOR) AT(f, FR_METHOD);
  vms->r->vm_effuid = (OBJECT) AT(f, FR_EFFUID);
}

PUBLIC INLINE void push_frame(VMSTATE vms) {
  OVECTOR newf = newovector(FR_MAXSLOTINDEX, T_FRAME);
  fillframe(vms, newf, vms->c.vm_ip);
  vms->r->vm_frame = newf;
}

PUBLIC INLINE void apply_closure(VMSTATE vms, OVECTOR closure, VECTOR argvec) {
  if (closure == NULL || TAGGEDP(closure)) {
    vm_raise(vms, (OBJ) newsym("invalid-callable"), (OBJ) closure);
  } else if (closure->type == T_PRIM) {
    int primargc;
    prim_fn fnp = lookup_prim(NUM(AT(closure, PR_NUMBER)), &primargc);

    if (fnp != NULL) {
      if ((primargc >= 0 && argvec->_.length-1 != primargc) ||
	  (primargc < 0 && argvec->_.length-1 < -primargc))
	vm_raise(vms, (OBJ) newsym("wrong-argc"), (OBJ) closure);
      else
	vms->r->vm_acc = fnp(vms, argvec);
    } else
      vm_raise(vms, (OBJ) newsym("invalid-primitive"), AT(closure, PR_NUMBER));
  } else if (closure->type == T_CLOSURE) {
    OVECTOR meth = (OVECTOR) AT(closure, CL_METHOD);

    if (!MS_CAN_X(meth, vms->r->vm_effuid)) {
      vm_raise(vms, (OBJ) newsym("no-permission"), AT(meth, ME_NAME));
      return;
    }

    if (argvec->_.length-1 != NUM(AT(meth, ME_ARGC))) {
      vm_raise(vms, (OBJ) newsym("wrong-argc"), (OBJ) meth);
      return;
    }

    push_frame(vms);

    vms->r->vm_env = argvec;
    ATPUT(vms->r->vm_env, 0, AT(meth, ME_ENV));

    vms->r->vm_lits = (VECTOR) AT(meth, ME_LITS);
    vms->r->vm_code = (BVECTOR) AT(meth, ME_CODE);
    vms->r->vm_self = (OBJECT) AT(closure, CL_SELF);
    vms->c.vm_ip = 0;
    vms->r->vm_method = meth;
    if (NUM(AT(meth, ME_FLAGS)) & O_SETUID)
      vms->r->vm_effuid = (OBJECT) AT(meth, ME_OWNER);
  } else if (closure->type == T_CONTINUATION) {
    int i;
    VECTOR cstk = (VECTOR) AT(closure, CONT_STACK);

    for (i = 0; i < cstk->_.length; i++)
      ATPUT(vms->r->vm_stack, i, AT(cstk, i));
    vms->c.vm_top = cstk->_.length;

    restoreframe(vms, (OVECTOR) AT(closure, CONT_FRAME));
    vms->r->vm_acc = AT(argvec, 1);
  } else {
    vm_raise(vms, (OBJ) newsym("invalid-callable"), (OBJ) closure);
  }
}

#ifdef DEBUG
#define INSTR0(n)	printf(n); ip++; break
#define INSTR1(n)	printf(n "%d", c[ip+1]); ip+=2; break
#define INSTR2(n)	printf(n "%d %d", c[ip+1], c[ip+2]); ip+=3; break
#define INSTR16(n)	printf(n "%d", (i16) ((int)c[ip+1]*256 + c[ip+2])); ip+=3; break
PRIVATE void debug_dump_instr(byte *c, int ip) {
  printf("%p %04d ", current_thread, ip);

  switch (c[ip]) {
    case OP_AT:			INSTR1("AT      ");
    case OP_ATPUT:		INSTR1("ATPUT   ");
    case OP_MOV_A_LOCL:		INSTR2("A<-LOCL ");
    case OP_MOV_A_GLOB:		INSTR1("A<-GLOB ");
    case OP_MOV_A_SLOT:		INSTR1("A<-SLOT ");
    case OP_MOV_A_LITL:		INSTR1("A<-LITL ");
    case OP_MOV_A_SELF:		INSTR0("A<-SELF ");
    case OP_MOV_A_FRAM:		INSTR0("A<-FRAM ");
    case OP_MOV_LOCL_A:		INSTR2("LOCL<-A ");
    case OP_MOV_GLOB_A:		INSTR1("GLOB<-A ");
    case OP_MOV_SLOT_A:		INSTR1("SLOT<-A ");
    case OP_MOV_FRAM_A:		INSTR0("FRAM<-A ");
    case OP_PUSH:		INSTR0("PUSH    ");
    case OP_POP:		INSTR0("POP     ");
    case OP_SWAP:		INSTR0("SWAP    ");
    case OP_VECTOR:		INSTR1("VECTOR  ");
    case OP_ENTER_SCOPE:	INSTR1("ENTER   ");
    case OP_LEAVE_SCOPE:	INSTR0("LEAVE   ");
    case OP_MAKE_VECTOR:	INSTR1("MKVECT  ");
    /*    case OP_FRAME:		INSTR16("FRAME   "); */
    case OP_CLOSURE:		INSTR0("CLOSURE ");
    case OP_METHOD_CLOSURE:	INSTR1("METHCLS ");
    case OP_RET:		INSTR0("RETURN  ");
    case OP_CALL:		INSTR1("CALL    ");
    case OP_CALL_AS:		INSTR1("CALLAS  ");
    case OP_APPLY:		INSTR0("APPLY   ");
    case OP_JUMP:		INSTR16("J       ");
    case OP_JUMP_TRUE:		INSTR16("JT      ");
    case OP_JUMP_FALSE:		INSTR16("JF      ");
    case OP_NOT:		INSTR0("NOT     ");
    case OP_EQ:			INSTR0("EQ      ");
    case OP_NE:			INSTR0("NE      ");
    case OP_GT:			INSTR0("GT      ");
    case OP_LT:			INSTR0("LT      ");
    case OP_GE:			INSTR0("GE      ");
    case OP_LE:			INSTR0("LE      ");
    case OP_NEG:		INSTR0("NEG     ");
    case OP_BNOT:		INSTR0("BNOT    ");
    case OP_BOR:		INSTR0("BOR     ");
    case OP_BAND:		INSTR0("BAND    ");
    case OP_PLUS:		INSTR0("PLUS    ");
    case OP_MINUS:		INSTR0("MINUS   ");
    case OP_STAR:		INSTR0("STAR    ");
    case OP_SLASH:		INSTR0("SLASH   ");
    case OP_PERCENT:		INSTR0("PERCENT ");
    default:
      printf("Unknown instr: %d", c[ip]); break;
  }
  printf("\n");
}
#endif

/************************************************************************/
/* Public parts								*/

PUBLIC char *checkpoint_filename;
PUBLIC uint32_t synch_bitmap;
PRIVATE int checkpoint_suffix;

PUBLIC void init_vm_global(void) {
  checkpoint_filename = NULL;
  synch_bitmap = 0;
  checkpoint_suffix = 0;
}

PUBLIC void vm_restore_from(FILE *f, int load_threads) {
  void *handle = start_load(f);

  symtab = (VECTOR) load(handle);
  if (load_threads)
    load_restartable_threads(handle, f);

  end_load(handle);
}

PRIVATE void checkpoint_now(void) {
  if (checkpoint_filename != NULL) {
    char buf[80];
    FILE *f;
    void *handle;

    sprintf(buf, "%s.%.3d", checkpoint_filename, checkpoint_suffix++);
    f = fopen(buf, "w");

    if (f == NULL)
      return;

    handle = start_save(f);

    save(handle, (OBJ) symtab);
    save_restartable_threads(handle, f);

    end_save(handle);
    fclose(f);
  }
}

PUBLIC void vm_poll_gc(void) {
  gc();
  synch_bitmap &= ~SYNCH_GC;

  if (synch_bitmap & SYNCH_CHECKPOINT) {
    printf("checkpointing...\n"); fflush(stdout);
    checkpoint_now();
    synch_bitmap &= ~SYNCH_CHECKPOINT;
  }
}

PUBLIC INLINE void gc_reach_safepoint(void) {
  if (need_gc() || (synch_bitmap & (SYNCH_GC | SYNCH_CHECKPOINT)))
    vm_poll_gc();
}

PUBLIC void init_vm(VMSTATE vms) {
  vms->r->vm_acc = NULL;
  vms->r->vm_code = NULL;
  vms->r->vm_env = NULL;
  vms->r->vm_lits = NULL;
  vms->r->vm_self = NULL;
  vms->r->vm_stack = newvector(VMSTACKLENGTH);
  vms->r->vm_frame = NULL;
  vms->r->vm_method = NULL;
  vms->c.vm_ip = vms->c.vm_top = 0;
  vms->r->vm_trap_closure = NULL;
  vms->r->vm_uid = NULL;
  vms->r->vm_effuid = NULL;
  vms->r->vm_locked = NULL;
  vms->c.vm_state = VM_DEFAULT_CPU_QUOTA;
  vms->c.vm_locked_count = 0;
}

PUBLIC void vm_raise(VMSTATE vms, OBJ exception, OBJ arg) {
  if (vms->r->vm_trap_closure != NULL) {
    VECTOR argvec = newvector_noinit(5);

    push_frame(vms);

#ifdef DEBUG
    printf("%p raising %s\n", current_thread, ((BVECTOR) AT((OVECTOR) exception, SY_NAME))->vec);
#endif

    ATPUT(argvec, 0, NULL);
    ATPUT(argvec, 1, exception);
    ATPUT(argvec, 2, arg);
    ATPUT(argvec, 3, (OBJ) getcont_from(vms));
    ATPUT(argvec, 4, vms->r->vm_acc);

    vms->c.vm_top = 0;
    apply_closure(vms, vms->r->vm_trap_closure, argvec);
    vms->r->vm_frame = NULL;	/* If it ever returns, the thread dies. */
  } else {
    if (OVECTORP(exception) && ((OVECTOR) exception)->type == T_SYMBOL)
      fprintf(stderr, "excp sym = '%s\n", ((BVECTOR) AT((OVECTOR) exception, SY_NAME))->vec);
    if (OVECTORP(arg) && ((OVECTOR) arg)->type == T_SYMBOL)
      fprintf(stderr, "arg sym = '%s\n", ((BVECTOR) AT((OVECTOR) arg, SY_NAME))->vec);
    if (BVECTORP(arg))
      fprintf(stderr, "arg str = %s\n", ((BVECTOR) arg)->vec);
    fprintf(stderr,
	    "Exception raised, no handler installed -> vm death.\n");
    vms->c.vm_state = VM_STATE_DYING;
  }
}

PRIVATE OBJ make_closure_from(OVECTOR method, OBJECT self, VECTOR env, OBJECT effuid) {
  OVECTOR nmeth = newovector(ME_MAXSLOTINDEX, T_METHOD);
  OVECTOR nclos = newovector(CL_MAXSLOTINDEX, T_CLOSURE);
  int i;

  for (i = 0; i < ME_MAXSLOTINDEX; i++)
    ATPUT(nmeth, i, AT(method, i));

  ATPUT(nmeth, ME_ENV, (OBJ) env);
  ATPUT(nmeth, ME_OWNER, (OBJ) effuid);
  ATPUT(nclos, CL_METHOD, (OBJ) nmeth);
  ATPUT(nclos, CL_SELF, (OBJ) self);
  return (OBJ) nclos;
}

PUBLIC BVECTOR bvector_concat(BVECTOR a, BVECTOR b) {
  BVECTOR n = newbvector_noinit(a->_.length + b->_.length);

  memcpy(n->vec, a->vec, a->_.length);
  memcpy(n->vec + a->_.length, b->vec, b->_.length);

  return n;
}

PUBLIC VECTOR vector_concat(VECTOR a, VECTOR b) {
  int alen = a->_.length;
  int blen = b->_.length;
  VECTOR n = newvector(alen + blen);
  int i, j;

  for (i = 0; i < alen; i++)
    ATPUT(n, i, AT(a, i));

  for (i = alen, j = 0; j < blen; i++, j++)
    ATPUT(n, i, AT(b, j));

  return n;
}

#define NUMOP(lab, exp) \
      case lab: \
	if (!NUMP(vms->r->vm_acc) || !NUMP(PEEK())) { \
	  vm_raise(vms, (OBJ) newsym("vm-runtime-type-error"), vms->r->vm_acc); \
	  break; \
	} \
	exp ; \
	vms->c.vm_ip++; \
	break

#define NOPERMISSION(x)	vm_raise(vms, (OBJ) newsym("no-permission"), x); break

PUBLIC int run_vm(VMSTATE vms) {
  OBJ vm_hold;	/* Holding register. NOT SEEN BY GC */
  int ticks_left = VM_TIMESLICE_TICKS;

  while (vms->c.vm_state != VM_STATE_DYING && ticks_left-- && vms->r->vm_acc != yield_thread) {
    if (vms->c.vm_state > 0) {
      vms->c.vm_state--;
      if (vms->c.vm_state == 0) {
	/* Quota expired. Warn. */
	vms->c.vm_state = VM_DEFAULT_CPU_QUOTA;
	vm_raise(vms, (OBJ) newsym("quota-expired"), NULL);
	/* Make sure we don't recurse :-) */
	vms->r->vm_trap_closure = NULL;
      }
    }

    gc_reach_safepoint();

#ifdef DEBUG
    debug_dump_instr( vms->r->vm_code->vec , vms->c.vm_ip );
#endif

    switch (CODEAT(vms->c.vm_ip)) {
      case OP_AT: {
	int index = CODEAT(vms->c.vm_ip + 1);

	if (index < 0 || index >= vms->r->vm_acc->length) {
	  vm_raise(vms, (OBJ) newsym("range-check-error"), vms->r->vm_acc);
	  break;
	}

	if (!VECTORP(vms->r->vm_acc)) {
	  vm_raise(vms, (OBJ) newsym("vm-runtime-type-error"), vms->r->vm_acc);
	  break;
	}

	vms->r->vm_acc = AT((VECTOR) vms->r->vm_acc, index);
	vms->c.vm_ip += 2;
	break;
      }

      case OP_ATPUT: {
	int index = CODEAT(vms->c.vm_ip + 1);

	vm_hold = PEEK();

	if (index < 0 || index >= vm_hold->length) {
	  vm_raise(vms, (OBJ) newsym("range-check-error"), vm_hold);
	  break;
	}

	if (!VECTORP(vm_hold)) {
	  vm_raise(vms, (OBJ) newsym("vm-runtime-type-error"), vm_hold);
	  break;
	}

	ATPUT((VECTOR) vm_hold, index, vms->r->vm_acc);
	vms->c.vm_ip += 2;
	break;
      }

      case OP_MOV_A_LOCL: {
	int i = CODEAT(vms->c.vm_ip + 1);
	vm_hold = (OBJ) vms->r->vm_env;
	while (i-- > 0) vm_hold = AT((VECTOR) vm_hold, 0);
	vms->r->vm_acc = AT((VECTOR) vm_hold, CODEAT(vms->c.vm_ip + 2) + 1);
	vms->c.vm_ip += 3;
	break;
      }

      case OP_MOV_A_GLOB:
	vm_hold = AT(vms->r->vm_lits, CODEAT(vms->c.vm_ip + 1));
	vms->r->vm_acc = AT((OVECTOR) vm_hold, SY_VALUE);
	vms->c.vm_ip += 2;
	break;

      case OP_MOV_A_SLOT: {
	OVECTOR slot, slotname;

	if (!OBJECTP(vms->r->vm_acc)) {
	  vm_raise(vms, (OBJ) newsym("vm-runtime-type-error"), vms->r->vm_acc);
	  break;
	}

	slotname = (OVECTOR) AT(vms->r->vm_lits, CODEAT(vms->c.vm_ip + 1));

	if (!O_CAN_X((OBJECT) vms->r->vm_acc, vms->r->vm_effuid)) {
	  NOPERMISSION((OBJ) slotname);
	}

	slot = findslot((OBJECT) vms->r->vm_acc, slotname, NULL);

	if (slot == NULL) {
	  vm_raise(vms, (OBJ) newsym("slot-not-found"), (OBJ) slotname);
	  break;
	}

	if (!MS_CAN_R(slot, vms->r->vm_effuid)) {
	  NOPERMISSION((OBJ) slotname);
	}

	vms->r->vm_acc = AT(slot, SL_VALUE);
	vms->c.vm_ip += 2;
	break;
      }

      case OP_MOV_A_LITL:
	vms->r->vm_acc = AT(vms->r->vm_lits, CODEAT(vms->c.vm_ip + 1));
	vms->c.vm_ip += 2;
	break;

      case OP_MOV_A_SELF: vms->r->vm_acc = (OBJ) vms->r->vm_self; vms->c.vm_ip++; break;
      case OP_MOV_A_FRAM: vms->r->vm_acc = (OBJ) vms->r->vm_frame; vms->c.vm_ip++; break;

      case OP_MOV_LOCL_A: {
	int i = CODEAT(vms->c.vm_ip + 1);
	vm_hold = (OBJ) vms->r->vm_env;
	while (i-- > 0) vm_hold = AT((VECTOR) vm_hold, 0);
	ATPUT((VECTOR) vm_hold, CODEAT(vms->c.vm_ip + 2) + 1, vms->r->vm_acc);
	vms->c.vm_ip += 3;
	break;
      }

      case OP_MOV_GLOB_A:
	if (!PRIVILEGEDP(vms->r->vm_effuid)) {
	  NOPERMISSION((OBJ) newsym("setting-global-value"));
	}
	vm_hold = AT(vms->r->vm_lits, CODEAT(vms->c.vm_ip + 1));
	ATPUT((OVECTOR) vm_hold, SY_VALUE, vms->r->vm_acc);
	vms->c.vm_ip += 2;
	break;

      case OP_MOV_SLOT_A: {
	OVECTOR slot, slotname;
	OBJECT target = (OBJECT) POP();
	OBJECT foundin;

	if (!OBJECTP(target)) {
	  vm_raise(vms, (OBJ) newsym("vm-runtime-type-error"), (OBJ) target);
	  break;
	}

	slotname = (OVECTOR) AT(vms->r->vm_lits, CODEAT(vms->c.vm_ip + 1));

	if (!O_CAN_X(target, vms->r->vm_effuid)) {
	  NOPERMISSION((OBJ) slotname);
	}

	slot = findslot(target, slotname, &foundin);

	if (slot == NULL) {
	  vm_raise(vms, (OBJ) newsym("slot-not-found"), (OBJ) slotname);
	  break;
	}

	if (!MS_CAN_W(slot, vms->r->vm_effuid)) {
	  NOPERMISSION((OBJ) slotname);
	}

	if (foundin == target) {
	  ATPUT(slot, SL_VALUE, vms->r->vm_acc);
	} else {
	  OVECTOR newslot = addslot(target, slotname, (OBJECT) AT(slot, SL_OWNER));
	  ATPUT(newslot, SL_FLAGS, AT(slot, SL_FLAGS));
	  ATPUT(newslot, SL_VALUE, vms->r->vm_acc);
	}

	vms->c.vm_ip += 2;
	break;
      }

      case OP_MOV_FRAM_A:
	if (!PRIVILEGEDP(vms->r->vm_effuid)) {
	  NOPERMISSION((OBJ) newsym("restoring-vm-frame-pointer"));
	}

	if (!OVECTORP(vms->r->vm_acc) || ((OVECTOR) vms->r->vm_acc)->type != T_FRAME) {
	  vm_raise(vms, (OBJ) newsym("vm-runtime-type-error"), vms->r->vm_acc);
	  break;
	}

	vms->r->vm_frame = (OVECTOR) vms->r->vm_acc;
	vms->c.vm_ip++;
	break;

      case OP_PUSH: PUSH(vms->r->vm_acc); vms->c.vm_ip++; break;
      case OP_POP: vms->r->vm_acc = POP(); vms->c.vm_ip++; break;
      case OP_SWAP:
	vm_hold = POP();
	PUSH(vms->r->vm_acc);
	vms->r->vm_acc = vm_hold;
	vms->c.vm_ip++;
	break;

      case OP_VECTOR:
	vms->r->vm_acc = (OBJ) newvector(CODEAT(vms->c.vm_ip+1));
	vms->c.vm_ip += 2;
	break;
	
      case OP_ENTER_SCOPE:
	vm_hold = (OBJ) newvector(CODEAT(vms->c.vm_ip+1) + 1);
	ATPUT((VECTOR) vm_hold, 0, (OBJ) vms->r->vm_env);
	vms->r->vm_env = (VECTOR) vm_hold;
	vms->c.vm_ip += 2;
	break;

      case OP_LEAVE_SCOPE:
	vms->r->vm_env = (VECTOR) AT(vms->r->vm_env, 0);
	vms->c.vm_ip++;
	break;

      case OP_MAKE_VECTOR: {
	int i = 0;
	int len = CODEAT(vms->c.vm_ip+1);
	VECTOR vec = newvector_noinit(len);

	for (i = len - 1; i >= 0; i--)
	  ATPUT(vec, i, POP());

	vms->r->vm_acc = (OBJ) vec;
	vms->c.vm_ip += 2;
	break;
      }

      case OP_CLOSURE:
	vms->r->vm_acc = make_closure_from((OVECTOR) vms->r->vm_acc,
					   vms->r->vm_self,
					   vms->r->vm_env,
					   vms->r->vm_effuid);
	vms->c.vm_ip++;
	break;

      case OP_METHOD_CLOSURE: {
	OVECTOR methname = (OVECTOR) AT(vms->r->vm_lits, CODEAT(vms->c.vm_ip + 1));
	OVECTOR method;

	if (!OBJECTP(vms->r->vm_acc)) {
	  vm_raise(vms, (OBJ) newsym("vm-runtime-type-error"), vms->r->vm_acc);
	  break;
	}

	method = findmethod((OBJECT) vms->r->vm_acc, methname);

	if (method == NULL) {
	  vm_raise(vms, (OBJ) newsym("method-not-found"), (OBJ) methname);
	  break;
	}

	if (!MS_CAN_R(method, vms->r->vm_effuid)) {
	  NOPERMISSION((OBJ) methname);
	}

	vm_hold = (OBJ) newovector(CL_MAXSLOTINDEX, T_CLOSURE);
	ATPUT((OVECTOR) vm_hold, CL_METHOD, (OBJ) method);
	ATPUT((OVECTOR) vm_hold, CL_SELF, vms->r->vm_acc);
	vms->r->vm_acc = vm_hold;

	vms->c.vm_ip += 2;
	break;
      }

      case OP_RET:
	if (vms->r->vm_frame != NULL) {
	  restoreframe(vms, vms->r->vm_frame);
	  if (vms->r->vm_code != NULL)
	    break;
	}

	vms->c.vm_state = VM_STATE_DYING;
	return 1;	/* finished, nothing more to run! */
	
      case OP_CALL: {
	OVECTOR methname = (OVECTOR) AT(vms->r->vm_lits, CODEAT(vms->c.vm_ip + 1));
	OVECTOR method;

	if (vms->r->vm_acc == NULL || TAGGEDP(vms->r->vm_acc)) {
	  vm_raise(vms,
		   (OBJ) newsym("null-call-error"),
		   AT(vms->r->vm_lits, CODEAT(vms->c.vm_ip+1)));
	  break;
	}

	if (!OBJECTP(vms->r->vm_acc)) {
	  vm_raise(vms, (OBJ) newsym("vm-runtime-type-error"), vms->r->vm_acc);
	  break;
	}

	method = findmethod((OBJECT) vms->r->vm_acc, methname);

	if (method == NULL) {
	  vm_raise(vms, (OBJ) newsym("method-not-found"), (OBJ) methname);
	  break;
	}

	if (!MS_CAN_X(method, vms->r->vm_effuid)) {
	  NOPERMISSION((OBJ) methname);
	}

	vm_hold = POP();
	if (vm_hold->length-1 != NUM(AT(method, ME_ARGC))) {
	  vm_raise(vms, (OBJ) newsym("wrong-argc"), (OBJ) methname);
	  break;
	}

	vms->c.vm_ip += 2;
	push_frame(vms);

	vms->r->vm_env = (VECTOR) vm_hold;
	ATPUT(vms->r->vm_env, 0, AT(method, ME_ENV));
	vms->r->vm_code = (BVECTOR) AT(method, ME_CODE);
	vms->r->vm_lits = (VECTOR) AT(method, ME_LITS);
	vms->r->vm_self = (OBJECT) vms->r->vm_acc;
	if (NUM(AT(method, ME_FLAGS)) & O_SETUID)
	  vms->r->vm_effuid = (OBJECT) AT(method, ME_OWNER);
	vms->r->vm_method = method;
	vms->c.vm_ip = 0;
	break;
      }

      case OP_CALL_AS: {
	OVECTOR methname = (OVECTOR) AT(vms->r->vm_lits, CODEAT(vms->c.vm_ip + 1));
	OVECTOR method;

	if (vms->r->vm_self == NULL ||
	    vms->r->vm_acc == NULL || TAGGEDP(vms->r->vm_acc)) {
	  vm_raise(vms,
		   (OBJ) newsym("null-call-error"),
		   AT(vms->r->vm_lits, CODEAT(vms->c.vm_ip+1)));
	  break;
	}

	if (!OBJECTP(vms->r->vm_acc)) {
	  vm_raise(vms, (OBJ) newsym("vm-runtime-type-error"), vms->r->vm_acc);
	  break;
	}

	method = findmethod((OBJECT) vms->r->vm_acc, methname);

	if (method == NULL) {
	  vm_raise(vms, (OBJ) newsym("method-not-found"), (OBJ) methname);
	  break;
	}

	if (!MS_CAN_X(method, vms->r->vm_effuid)) {
	  NOPERMISSION((OBJ) methname);
	}

	vm_hold = POP();
	if (vm_hold->length-1 != NUM(AT(method, ME_ARGC))) {
	  vm_raise(vms, (OBJ) newsym("wrong-argc"), (OBJ) methname);
	  break;
	}

	vms->c.vm_ip += 2;
	push_frame(vms);

	vms->r->vm_env = (VECTOR) vm_hold;
	ATPUT(vms->r->vm_env, 0, AT(method, ME_ENV));
	vms->r->vm_code = (BVECTOR) AT(method, ME_CODE);
	vms->r->vm_lits = (VECTOR) AT(method, ME_LITS);

	/* don't set vm_self, this is OP_CALL_AS. */
	/* vms->r->vm_self = vms->r->vm_acc; */

	if (NUM(AT(method, ME_FLAGS)) & O_SETUID)
	  vms->r->vm_effuid = (OBJECT) AT(method, ME_OWNER);
	vms->r->vm_method = method;
	vms->c.vm_ip = 0;
	break;
      }

      case OP_APPLY:
	vms->c.vm_ip++;
	apply_closure(vms, (OVECTOR) vms->r->vm_acc, (VECTOR) POP());
	break;

      case OP_JUMP: vms->c.vm_ip += 3 + ((int16_t) CODE16AT(vms->c.vm_ip+1)); break;

      case OP_JUMP_TRUE:
	vms->c.vm_ip += (vms->r->vm_acc == false) ? 3 :
						    3 + ((int16_t) CODE16AT(vms->c.vm_ip+1));
	break;

      case OP_JUMP_FALSE:
	vms->c.vm_ip += (vms->r->vm_acc != false) ? 3 :
						    3 + ((int16_t) CODE16AT(vms->c.vm_ip+1));
	break;

      case OP_NOT:
	vms->r->vm_acc = (vms->r->vm_acc == false) ? true : false;
	vms->c.vm_ip++;
	break;

      case OP_EQ:
	vms->r->vm_acc = (vms->r->vm_acc == POP()) ? true : false;
	vms->c.vm_ip++;
	break;

      case OP_NE:
	vms->r->vm_acc = (vms->r->vm_acc != POP()) ? true : false;
	vms->c.vm_ip++;
	break;

      NUMOP(OP_GT, vms->r->vm_acc = (NUM(vms->r->vm_acc) < NUM(POP())) ? true : false);
      NUMOP(OP_LT, vms->r->vm_acc = (NUM(vms->r->vm_acc) > NUM(POP())) ? true : false);
      NUMOP(OP_GE, vms->r->vm_acc = (NUM(vms->r->vm_acc) <= NUM(POP())) ? true : false);
      NUMOP(OP_LE, vms->r->vm_acc = (NUM(vms->r->vm_acc) >= NUM(POP())) ? true : false);

      NUMOP(OP_NEG, vms->r->vm_acc = MKNUM(-NUM(vms->r->vm_acc)));
      NUMOP(OP_BNOT, vms->r->vm_acc = MKNUM(~NUM(vms->r->vm_acc)));
      NUMOP(OP_BOR, vms->r->vm_acc = MKNUM(NUM(vms->r->vm_acc)|NUM(POP())));
      NUMOP(OP_BAND, vms->r->vm_acc = MKNUM(NUM(vms->r->vm_acc)&NUM(POP())));

      case OP_PLUS:
	if (vms->r->vm_acc == NULL || PEEK() == NULL) {
	  vm_raise(vms, (OBJ) newsym("vm-runtime-type-error"), vms->r->vm_acc);
	  break;
	}
	if (NUMP(vms->r->vm_acc) && NUMP(PEEK()))
	  vms->r->vm_acc = MKNUM(NUM(vms->r->vm_acc)+NUM(POP()));
	else if (TAGGEDP(vms->r->vm_acc) || TAGGEDP(PEEK())) {
	  vm_raise(vms, (OBJ) newsym("vm-runtime-type-error"), vms->r->vm_acc);
	  break;
	} else if (BVECTORP(vms->r->vm_acc) && BVECTORP(PEEK()))
	  vms->r->vm_acc = (OBJ) bvector_concat((BVECTOR) POP(), (BVECTOR) vms->r->vm_acc);
	else if (VECTORP(vms->r->vm_acc) && VECTORP(PEEK()))
	  vms->r->vm_acc = (OBJ) vector_concat((VECTOR) POP(), (VECTOR) vms->r->vm_acc);
	else {
	  vm_raise(vms, (OBJ) newsym("vm-runtime-type-error"), vms->r->vm_acc);
	  break;
	}
	vms->c.vm_ip++;
	break;

      NUMOP(OP_MINUS, vms->r->vm_acc = MKNUM(NUM(POP())-NUM(vms->r->vm_acc)));
      NUMOP(OP_STAR, vms->r->vm_acc = MKNUM(NUM(POP())*NUM(vms->r->vm_acc)));
      NUMOP(OP_SLASH,
	    if (vms->r->vm_acc == MKNUM(0))
	      vm_raise(vms, (OBJ) newsym("divide-by-zero"), NULL);
	    else
	      vms->r->vm_acc = MKNUM(NUM(POP())/NUM(vms->r->vm_acc)));
      NUMOP(OP_PERCENT,
	    if (vms->r->vm_acc == MKNUM(0))
	      vm_raise(vms, (OBJ) newsym("divide-by-zero"), NULL);
	    else
	      vms->r->vm_acc = MKNUM(NUM(POP())%NUM(vms->r->vm_acc)));

      default:
	fprintf(stderr, "Unknown bytecode reached (%d == 0x%x).\n",
		CODEAT(vms->c.vm_ip),
		CODEAT(vms->c.vm_ip));
	exit(MOVE_EXIT_PROGRAMMER_FUCKUP);
    }
  }

  return vms->c.vm_state == VM_STATE_DYING;
}

PUBLIC OVECTOR getcont_from(VMSTATE vms) {
  OVECTOR cont;
  VECTOR cstk;
  int i;

  cont = newovector_noinit(CONT_MAXSLOTINDEX, T_CONTINUATION);
  ATPUT(cont, CONT_FRAME, (OBJ) vms->r->vm_frame);
  cstk = newvector_noinit(vms->c.vm_top);
  ATPUT(cont, CONT_STACK, (OBJ) cstk);

  for (i = 0; i < vms->c.vm_top; i++)
    ATPUT(cstk, i, AT(vms->r->vm_stack, i));

  return cont;
}
