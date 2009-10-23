/*
 * 3-MOVE, a multi-user networked online text-based programmable virtual environment
 * Copyright 1997, 1998, 1999, 2003, 2005, 2008, 2009 Tony Garnock-Jones <tonyg@kcbbs.gen.nz>
 *
 * 3-MOVE is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * 3-MOVE is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with 3-MOVE.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef Prim_H
#define Prim_H

typedef OBJ (*prim_fn)(VMSTATE, VECTOR);

extern void init_prim(void);

extern void register_prim(int primargc, char *name, int number, prim_fn fn);
extern void bind_primitives_to_symbols(void);
extern prim_fn lookup_prim(int number, int *primargc);

#ifdef DEFINING_MOVE_PRIMITIVES
/* Utilities for defining primitive functions. */

# define TYPEERRIF(c)	if (c) {							\
			  vm_raise(vms, (OBJ) newsym("type-error"), (OBJ) argvec);	\
			  return undefined;						\
			}

# define DEFPRIM(name)	PRIVATE OBJ name(VMSTATE vms, VECTOR argvec)
# define ARG(n)		AT(argvec, (n)+1)

#endif

#endif
