#ifndef Prim_H
#define Prim_H

typedef OBJ (*prim_fn)(VMSTATE, VECTOR);

extern void init_prim(void);

extern void register_prim(int primargc, char *name, int number, prim_fn fn);
extern void bind_primitives_to_symbols(void);
extern prim_fn lookup_prim(int number, int *primargc);

#ifdef DEFINING_MOVE_PRIMITIVES
/* Utilities for defining primitive functions. */

# define TYPEERRIF(c)	if (c) {							\
			  vm_raise(vms, (OBJ) newsym("type-error"), (OBJ) argvec);	\
			  return undefined;						\
			}

# define DEFPRIM(name)	PRIVATE OBJ name(VMSTATE vms, VECTOR argvec)
# define ARG(n)		AT(argvec, (n)+1)

#endif

#endif
