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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "buffer.h"

#include "global.h"
#include "object.h"
#include "gc.h"

BUFFER newbuf(int initial_length) {
  BUFFER buf = allocmem(sizeof(Buffer));

  buf->buflength = initial_length;
  buf->pos = 0;
  buf->buf = allocmem(initial_length);

  return buf;
}

void killbuf(BUFFER buf) {
  freemem(buf->buf);
  freemem(buf);
}

void buf_append(BUFFER buf, char ch) {
  if (buf->pos >= buf->buflength) {
    char *newbuf = allocmem(buf->buflength + 128);

    if (newbuf == NULL) {
      fprintf(stderr, "buf_append: could not grow buffer\n");
      exit(MOVE_EXIT_MEMORY_ODDNESS);
    }

    memcpy(newbuf, buf->buf, buf->buflength);
    freemem(buf->buf);
    buf->buf = newbuf;
    buf->buflength += 128;
  }

  buf->buf[buf->pos++] = ch;
}

