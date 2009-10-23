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

#ifndef Scanner_H
#define Scanner_H

#define SCAN_EOF	0

#define USER_SYM	256

#define LITERAL		(USER_SYM + 0)
#define IDENT		(USER_SYM + 1)

#define K_DEFINE	(USER_SYM + 2)
#define K_METHOD	(USER_SYM + 3)
#define K_IF		(USER_SYM + 4)
#define K_ELSE		(USER_SYM + 5)
#define K_WHILE		(USER_SYM + 6)
#define K_DO		(USER_SYM + 7)
#define K_RETURN	(USER_SYM + 8)
#define K_FUNCTION	(USER_SYM + 9)
#define K_BIND_CC	(USER_SYM + 10)
#define K_AS		(USER_SYM + 11)
#define K_THIS		(USER_SYM + 12)

typedef int (*stream_fn)(void *arg);

typedef struct ScanInst {
  stream_fn getter;
  void *arg;
  int cache;
  int linenum;
  OBJ yylval;
} ScanInst, *SCANINST;

extern int scan(SCANINST scaninst);

#endif
