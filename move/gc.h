#ifndef GC_H
#define GC_H

extern void init_gc(void);
extern void done_gc(void);

extern void protect(OBJ *root);
extern void unprotect(OBJ *root);

extern OBJ next_to_finalize(void);	/* Gets the next object ready for finalization */
extern void wait_for_finalize(void);	/* Waits for an object to become ready to finalize */
extern void awaken_finalizer(void);	/* Forces wait_for_finalize to return */

extern void gc(void);			/* Performs a mark-sweep non-copying non-compacting GC */
extern int need_gc(void);		/* Is a GC necessary? */
extern OBJ getmem(int size, int kind, int length);
					/* Allocates size bytes of object space, with
					   kind 'kind' and length 'length'. */

extern void *allocmem(int size);	/* Allocates size bytes of non-object space */
extern void freemem(void *mem);		/* Frees non-object space */

#endif
