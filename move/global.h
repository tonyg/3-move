#ifndef Global_H
#define Global_H

#include <pthread.h>
#include "config.h"
#include "recmutex.h"

#define PRIVATE static
#define PUBLIC
#define INLINE inline

typedef unsigned int word;

typedef unsigned char byte;
typedef unsigned short u16;
typedef signed short i16;
typedef unsigned int u32;	/* NOTE: on Alpha this may have to be unsigned long! */
typedef signed int i32;		/* NOTE: on Alpha this may have to be signed long! */
				/* In any case, should match the native sizeof(void *). */

#endif
