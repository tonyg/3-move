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

#ifndef Persist_H
#define Persist_H

#include <stdio.h>

#define DBFMT_OLDFMT		0	/* Old, machine-specific, yucky format */
#define DBFMT_BIG_32_OLD	1	/* Mostly big-endian (network-order) 32 bit format */
#define DBFMT_NET_32		2	/* Entirely network-order 32 bit format */

#define DEFAULT_DBFMT		DBFMT_NET_32

#define DBFMT_SIGNATURE		"MOVEdatabase"	/* First 32 bytes of file are
						   MOVEdatabaseXXXX, XXXX being an
						   unsigned bigendian version number. */
#define DBFMT_SIG_LEN		(sizeof(DBFMT_SIGNATURE) - 1)	/* for the trailing \0. */

extern void *start_save(FILE *dest);
extern void end_save(void *handle);
extern void save(void *handle, OBJ root);

extern void *start_load(FILE *source);
extern void end_load(void *handle);
extern OBJ load(void *handle);

extern int get_handle_dbfmt(void *handle);

/* Used by thread.c during loading: */
extern void checked_fread(void *ptr, size_t size, size_t nmemb, FILE *stream);

#endif
