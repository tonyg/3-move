#ifndef Prim_H
#define Prim_H

typedef OBJ (*prim_fn)(VMSTATE, VECTOR);

extern void init_prim(void);

extern void register_prim(int primargc, char *name, int number, prim_fn fn);
extern void bind_primitives_to_symbols(void);
extern prim_fn lookup_prim(int number, int *primargc);

#endif
