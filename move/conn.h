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

#ifndef Conn_H
#define Conn_H

extern OVECTOR newfileconn(int fd);
extern OVECTOR newstringconn(BVECTOR data);

#define CONN_RET_ERROR		-1	/* EOF or normal error-with-errno */
#define CONN_RET_BLOCK		-2	/* thread blocked, yield asap */

extern int conn_write(const char *buf, int size, OVECTOR conn);
extern int conn_puts(const char *s, OVECTOR conn);

extern void conn_close(OVECTOR conn);
#define conn_closed(conn)	((conn) == NULL || NUM(AT(conn, CO_TYPE)) == CONN_NONE)

extern void fill_scaninst(SCANINST si, OVECTOR conn);

extern int conn_resume_readline(VECTOR args);
extern int conn_resume_accept(OVECTOR server);

#endif
