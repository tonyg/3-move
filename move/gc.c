#include "global.h"
#include "object.h"
#include "pair.h"

#include "barrier.h"
#include "gc.h"
#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#if 0
#define DEBUG
#endif

typedef struct Root *ROOT;
typedef struct Root {
  ROOT next;
  OBJ *root;
} Root;

PRIVATE pthread_mutex_t roots_mutex;	/* Protects protect() and unprotect(),
					   also the mark phase of gc() */

PUBLIC pthread_mutex_t heap_mutex;	/* Protects getmem(), allocmem() and freemem(),
					   also gc() */

PRIVATE pthread_mutex_t chain_mutex;	/* Protects access to all_objects and finalizable_chain */
PRIVATE pthread_cond_t fin_chain_signal;	/* Signals when there's something to
						   finalize */

PRIVATE int num_roots;
PRIVATE ROOT roots;			/* Chain of all markable roots */
PRIVATE OBJ all_objects;		/* Chain of all objects */
PRIVATE OBJ finalizable_chain;		/* Chain of objects to finalize */

PRIVATE int max_heapsize;		/* When heap reaches this size, gc occurs */
PRIVATE int double_threshold;		/* When heap is this big after a gc, max_heapsize
					   and double_threshold are doubled. */
PRIVATE int curr_heapsize;		/* Currently allocated bytes of heap */

PUBLIC void init_gc(void) {
  pthread_mutex_init(&roots_mutex, NULL);
  pthread_mutex_init(&heap_mutex, NULL);
  pthread_mutex_init(&chain_mutex, NULL);
  pthread_cond_init(&fin_chain_signal, NULL);

  num_roots = 0;
  roots = NULL;

  all_objects = NULL;
  finalizable_chain = NULL;

  max_heapsize = GC_MAX_HEAPSIZE;
  double_threshold = GC_DOUBLE_THRESHOLD;
  curr_heapsize = 0;
}

PUBLIC void done_gc(void) {
  while (all_objects != NULL) {
    OBJ x = all_objects;
    all_objects = all_objects->next;
    free(x);
  }

  while (roots != NULL) {
    ROOT r = roots;
    roots = roots->next;
    free(r);
  }
}

PUBLIC void protect(OBJ *root) {
  ROOT r = allocmem(sizeof(Root));

  if (r == NULL)
    exit(1);

  pthread_mutex_lock(&roots_mutex);
#ifdef DEBUG
  printf("%d protecting %p\n", pthread_self(), root);
#endif
  r->next = roots;
  r->root = root;
  roots = r;
  num_roots++;
  pthread_mutex_unlock(&roots_mutex);
}

PUBLIC void unprotect(OBJ *root) {
  ROOT r;
  ROOT prev = NULL;

  pthread_mutex_lock(&roots_mutex);
#ifdef DEBUG
  printf("%d unprotecting %p\n", pthread_self(), root);
#endif
  r = roots;

  while (r != NULL) {
    if (r->root == root) {
      if (prev == NULL)
	roots = r->next;
      else
	prev->next = r->next;
      freemem(r);
      num_roots--;
      pthread_mutex_unlock(&roots_mutex);
      return;
    }
    prev = r;
    r = r->next;
  }

  pthread_mutex_unlock(&roots_mutex);
}

PRIVATE INLINE int object_length_bytes(OBJ x) {
  switch (x->kind) {
    case KIND_OBJECT:
      return sizeof(Object);

    case KIND_BVECTOR:
      return sizeof(BVector) + x->length;

    case KIND_VECTOR:
      return sizeof(Vector) + x->length * sizeof(OBJ);

    case KIND_OVECTOR:
      return sizeof(OVector) + x->length * sizeof(OBJ);
  }

  return 0;
}

PUBLIC OBJ next_to_finalize(void) {
  OBJ result;

  pthread_mutex_lock(&chain_mutex);

  result = finalizable_chain;
  if (result != NULL) {
    finalizable_chain = result->next;
    result->next = all_objects;
    all_objects = result;
    curr_heapsize += object_length_bytes(result);
  }

  pthread_mutex_unlock(&chain_mutex);

  return result;
}

PUBLIC void wait_for_finalize(void) {
  pthread_mutex_lock(&chain_mutex);
  pthread_cond_wait(&fin_chain_signal, &chain_mutex);
  pthread_mutex_unlock(&chain_mutex);
}

PUBLIC void awaken_finalizer(void) {
  pthread_mutex_lock(&chain_mutex);
  pthread_cond_broadcast(&fin_chain_signal);
  pthread_mutex_unlock(&chain_mutex);
}

PRIVATE void mark(OBJ x) {
  if (x == NULL || TAGGEDP(x))
    return;

  if (x->marked)
    return;

  x->marked = 1;

  switch (x->kind) {
    case KIND_OBJECT: {
      OBJECT ox = (OBJECT) x;

      mark((OBJ) ox->methods);
      mark((OBJ) ox->attributes);
      mark(ox->parents);
      mark((OBJ) ox->owner);
      mark((OBJ) ox->group);
      mark((OBJ) ox->location);
      mark((OBJ) ox->contents);
      break;
    }

    case KIND_BVECTOR:
      break;

    case KIND_OVECTOR: {
      OVECTOR ox = (OVECTOR) x;
      int i;

      if (ox->type == T_SYMBOL) {
	for (i = 0; i < x->length; i++)
	  if (i != SY_NEXT)
	    mark(AT(ox, i));
      } else {
	for (i = 0; i < x->length; i++)
	  mark(AT(ox, i));
      }

      break;
    }

    case KIND_VECTOR: {
      VECTOR vx = (VECTOR) x;
      int i;

      for (i = 0; i < x->length; i++)
	mark(AT(vx, i));
      break;
    }
  }
}

PUBLIC void gc(void) {
  ROOT r;
  OBJ x;

  /* Grab every resource related to GC. */
  pthread_mutex_lock(&chain_mutex);
  pthread_mutex_lock(&roots_mutex);
  pthread_mutex_lock(&heap_mutex);

#ifdef DEBUG
  printf("-----GC-----"); fflush(stdout);
#endif

  r = roots;

  while (r != NULL) {
#ifdef DEBUG
    printf("%d marking %p: %p\n", pthread_self(), r->root, *(r->root));
#endif
    mark(*(r->root));
    r = r->next;
  }

  {
    int i;

    symtab->_.marked = 1;
    for (i = 0; i < symtab->_.length; i++) {
      OVECTOR curr = (OVECTOR) AT(symtab, i);

      while (curr != NULL) {
	if (!curr->_.marked && AT(curr, SY_VALUE) != undefined)
	  mark((OBJ) curr);

	curr = (OVECTOR) AT(curr, SY_NEXT);
      }

      ATPUT(symtab, i, NULL);
    }
  }

  x = all_objects;
  all_objects = NULL;
  curr_heapsize = 0;

  while (x != NULL) {
    OBJ n = x->next;

    if (!x->marked) {
      if (OBJECTP(x) && ((OBJECT) x)->finalize) {
	((OBJECT) x)->finalize = 0;
	x->next = finalizable_chain;
	finalizable_chain = x;
      } else if (OVECTORP(x) && ((OVECTOR) x)->finalize) {
	((OVECTOR) x)->finalize = 0;
	x->next = finalizable_chain;
	finalizable_chain = x;
      } else {
	free(x);
      }
    } else {
      if (OVECTORP(x) && ((OVECTOR) x)->type == T_SYMBOL) {
	OVECTOR sym = (OVECTOR) x;
	u32 h = UNUM(AT(sym, SY_HASH)) % symtab->_.length;
	ATPUT(sym, SY_NEXT, AT(symtab, h));
	ATPUT(symtab, h, x);
      }

      x->marked = 0;
      x->next = all_objects;
      all_objects = x;
      curr_heapsize += object_length_bytes(x);
    }

    x = n;
  }

  if (curr_heapsize > double_threshold) {
    max_heapsize <<= 1;
    double_threshold <<= 1;
  }

  if (finalizable_chain != NULL)
    pthread_cond_broadcast(&fin_chain_signal);

  pthread_mutex_unlock(&heap_mutex);
  pthread_mutex_unlock(&roots_mutex);
  pthread_mutex_unlock(&chain_mutex);

#ifdef DEBUG
  printf("GC-ENDS\n");
  fflush(stdout);
#endif
}

PUBLIC int need_gc(void) {
  return curr_heapsize > max_heapsize;
}

PUBLIC OBJ getmem(int size, int kind, int length) {
  OBJ ptr;

  pthread_mutex_lock(&heap_mutex);
  ptr = malloc(size);
  pthread_mutex_unlock(&heap_mutex);

  if (ptr == NULL) {
    fprintf(stderr, "fatal error: malloc returned NULL, size == %d\n", size);
    exit(1);
  }

  pthread_mutex_lock(&chain_mutex);
  ptr->next = all_objects;
  ptr->kind = kind;
  ptr->marked = 0;
  ptr->length = length;
  all_objects = ptr;
  curr_heapsize += size;
  pthread_mutex_unlock(&chain_mutex);

  return ptr;
}

PUBLIC void *allocmem(int size) {
  void *ptr;

  pthread_mutex_lock(&heap_mutex);
  ptr = malloc(size);
  pthread_mutex_unlock(&heap_mutex);

  if (ptr == NULL) {
    fprintf(stderr, "fatal error: malloc returned NULL, size == %d\n", size);
    exit(1);
  }

  return ptr;
}

PUBLIC void freemem(void *mem) {
  pthread_mutex_lock(&heap_mutex);
  free(mem);
  pthread_mutex_unlock(&heap_mutex);
}
