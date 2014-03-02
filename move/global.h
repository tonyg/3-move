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

#ifndef Global_H
#define Global_H

#include <stdlib.h>
#include <stdint.h>

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

typedef uintptr_t word;
typedef uintptr_t unum;
typedef intptr_t inum;

#endif
