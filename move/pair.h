#ifndef Pair_H
#define Pair_H

#define CONS(v, x, y)	((void) (v = newvector_noinit(2), SETCAR(v, x), SETCDR(v, y), v))

#define CAR(x)		AT(x, 0)
#define CDR(x)		AT(x, 1)
#define SETCAR(x, y)	ATPUT(x, 0, y)
#define SETCDR(x, y)	ATPUT(x, 1, y)

#endif
