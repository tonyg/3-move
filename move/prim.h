#ifndef Prim_H
#define Prim_H

typedef OBJ (*prim_fn)(VMSTATE, VECTOR);

extern void init_prim(void);

extern void register_prim(char *name, int number, prim_fn fn);
extern prim_fn lookup_prim(int number);

#endif
