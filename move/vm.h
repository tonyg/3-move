#ifndef VM_H
#define VM_H

/************************************************************************/
/* VM register file							*/

#define NUM_VMREGS	13
typedef struct VMregs {
  Obj		_;

  OBJ		vm_acc;		/* Accumulator */
  BVECTOR	vm_code;	/* Code block */
  VECTOR	vm_env;		/* Environment */
  VECTOR	vm_lits;	/* Literal table */
  OBJECT	vm_self;	/* Self object */
  VECTOR	vm_stack;	/* Stack */
  OVECTOR	vm_frame;	/* Frame stack */
  OVECTOR	vm_method;	/* The callable that this frame is executing */
  OVECTOR	vm_trap_closure;	/* closure to call on VM trap, eg vm_raise etc. */
  OBJECT	vm_uid;		/* Real owner of this thread */
  OBJECT	vm_effuid;	/* Effective owner of this code */

  OVECTOR	vm_input;	/* Input connection */
  OVECTOR	vm_output;	/* Output connection */
} VMregs, *VMREGS;

typedef struct VMregs_C {
  i32		vm_ip;		/* Instruction pointer */
  i32		vm_top;		/* Top of stack (ascending, empty) */
  int		vm_state;	/* Status of Virtual Machine - 1 = alive, 0 = dying */
} VMregs_C, *VMREGS_C;

typedef struct VMstate {
  VMREGS r;
  VMregs_C c;
} VMstate, *VMSTATE;

/************************************************************************/
/* VM API								*/

extern BVECTOR bvector_concat(BVECTOR a, BVECTOR b);
extern VECTOR vector_concat(VECTOR a, VECTOR b);

extern void init_vm_global(void);
extern void *vm_gc_thread_main(void *arg);
extern void make_gc_thread_exit(void);
extern void gc_inc_safepoints(void);
extern void gc_dec_safepoints(void);
extern void gc_reach_safepoint(void);

extern void init_vm(VMSTATE vms);

extern void vm_raise(VMSTATE vms, OBJ exception, OBJ arg);
extern void run_vm(VMSTATE vms, OBJECT self, OVECTOR method);

#endif
