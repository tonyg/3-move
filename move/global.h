#ifndef Global_H
#define Global_H

#include <stdlib.h>

#include "config.h"
#include "recmutex.h"

#define MOVE_EXIT_OK			0	/* move run successfully until told to quit */
#define MOVE_EXIT_ERROR			1	/* some general fatal error condition */
#define MOVE_EXIT_INTERRUPTED		2	/* SIGINT and other nasty user interrupts */
#define MOVE_EXIT_MEMORY_ODDNESS	3	/* some memory bug/wierdness */
#define MOVE_EXIT_MEMORY_EXHAUSTED	4	/* no memory left for move */
#define MOVE_EXIT_DBFMT_ERROR		5	/* disk database wierdness */
#define MOVE_EXIT_PROGRAMMER_FUCKUP	64	/* shouldn't ever happen. */

#define PRIVATE static
#define PUBLIC

#ifdef __GNUC__
#define INLINE inline
#else
#define INLINE
#endif

typedef unsigned int word;

typedef unsigned char byte;
typedef unsigned short u16;
typedef signed short i16;
typedef unsigned int u32;
typedef signed int i32;

/* *********************************************************** */
/* **************************  NOTE  ************************* */
/* sizeof(u32) and sizeof(i32) should match the native
   sizeof(void *). This is for the pointer-tagging. */

#if 0	/* These defs are for Alpha. */
typedef unsigned long unum;
typedef signed long inum;
#endif

#if 1	/* These defs are for Linux/i386. */
typedef unsigned int unum;
typedef signed int inum;
#endif

#endif
