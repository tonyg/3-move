#ifndef Config_H
#define Config_H

#define HAVE_NATIVE_RECURSIVE_MUTEXES	0

/* For the timeslice:
   500 was too jerky.
   */
#define VM_TIMESLICE_TICKS		2000	/* reasonable? who knows. */

/* These numbers need to be measured to see if they are useful in
   practice. */

#define GC_MAX_HEAPSIZE		1048576		/* 1 MB */
#define GC_DOUBLE_THRESHOLD	786432		/* 0.75 MB */

#endif
