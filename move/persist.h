#ifndef Persist_H
#define Persist_H

#include <stdio.h>

extern void *start_save(FILE *dest);
extern void end_save(void *handle);
extern void save(void *handle, OBJ root);

extern void *start_load(FILE *source);
extern void end_load(void *handle);
extern OBJ load(void *handle);

#endif
