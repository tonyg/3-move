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

#ifndef Pair_H
#define Pair_H

#define CONS(v, x, y)	((void) (v = newvector_noinit(2), SETCAR(v, x), SETCDR(v, y), v))

#define CAR(x)		AT(x, 0)
#define CDR(x)		AT(x, 1)
#define SETCAR(x, y)	ATPUT(x, 0, y)
#define SETCDR(x, y)	ATPUT(x, 1, y)

#endif
