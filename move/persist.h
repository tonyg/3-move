#ifndef Persist_H
#define Persist_H

#include <stdio.h>

#define DBFMT_OLDFMT		0	/* Old, machine-specific, yucky format */
#define DBFMT_BIG_32		1	/* Big-endian 32 bit format */

#define DBFMT_SIGNATURE		"MOVEdatabase"	/* First 32 bytes of file are
						   MOVEdatabaseXXXX, XXXX being an
						   unsigned bigendian version number. */
#define DBFMT_SIG_LEN		(sizeof(DBFMT_SIGNATURE) - 1)	/* for the trailing \0. */

extern void *start_save(FILE *dest);
extern void end_save(void *handle);
extern void save(void *handle, OBJ root);

extern void *start_load(FILE *source);
extern void end_load(void *handle);
extern OBJ load(void *handle);

#endif
