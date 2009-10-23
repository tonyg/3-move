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
