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
