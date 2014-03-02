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
#include <errno.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "global.h"
#include "object.h"
#include "scanner.h"
#include "conn.h"
#include "vm.h"
#include "thread.h"
#include "gc.h"

#if 0
#define DEBUG
#endif

PRIVATE INLINE OVECTOR mkfileconn(int fd, int blocking) {
  OVECTOR c = newovector(CO_MAXSLOTINDEX, T_CONNECTION);

  if (!blocking)
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

  c->finalize = 1;
  ATPUT(c, CO_TYPE, MKNUM(CONN_FILE));
  ATPUT(c, CO_UNGETC, MKNUM(-1));
  ATPUT(c, CO_INFO, NULL);
  ATPUT(c, CO_HANDLE, MKNUM(fd));

  return c;
}

PUBLIC OVECTOR newfileconn(int fd) {
  return mkfileconn(fd, 0);
}

PUBLIC OVECTOR newstringconn(BVECTOR data) {
  OVECTOR c = newovector(CO_MAXSLOTINDEX, T_CONNECTION);

  c->finalize = 1;
  ATPUT(c, CO_TYPE, MKNUM(CONN_STRING));
  ATPUT(c, CO_UNGETC, MKNUM(-1));
  ATPUT(c, CO_INFO, MKNUM(0));
  ATPUT(c, CO_HANDLE, (OBJ) data);

  return c;
}

PRIVATE void conn_ungetc(OVECTOR conn, int ch) {
  ATPUT(conn, CO_UNGETC, MKNUM(ch));
}

PRIVATE int stringconn_getter(OVECTOR conn) {
  BVECTOR data = (BVECTOR) AT(conn, CO_HANDLE);
  int pos = NUM(AT(conn, CO_INFO));

  if (conn_closed(conn))
    return CONN_RET_ERROR;

  if (NUM(AT(conn, CO_UNGETC)) != -1) {
    int u = NUM(AT(conn, CO_UNGETC));
    ATPUT(conn, CO_UNGETC, MKNUM(-1));
    return u;
  }

  if (pos >= data->_.length) {
    conn_close(conn);
    return CONN_RET_ERROR;
  }

  ATPUT(conn, CO_INFO, MKNUM(pos+1));
  return AT(data, pos);
}

PRIVATE int fileconn_getter(OVECTOR conn) {
  char buf;

  if (conn_closed(conn))
    return CONN_RET_ERROR;

  if (NUM(AT(conn, CO_UNGETC)) >= 0) {
    int u = NUM(AT(conn, CO_UNGETC));
    ATPUT(conn, CO_UNGETC, MKNUM(-1));
    return u;
  }

  while (1) {
    switch (read(NUM(AT(conn, CO_HANDLE)), &buf, 1)) {
      case -1:
	if (errno == EAGAIN)
	  return CONN_RET_BLOCK;

	return CONN_RET_ERROR;

      case 0:
	conn_close(conn);
	return CONN_RET_ERROR;

      default:	/* Read was successful. */
	/* This next bit of hackery is to support not only UNIX line-termination conventions,
	   but also *both* Mac *and* MS-DOS/Windows. Hurrah. */

	if (NUM(AT(conn, CO_UNGETC)) == -2 && buf == '\n') {	/* "eat next nl." */
	  ATPUT(conn, CO_UNGETC, MKNUM(-1));
	  continue;	/* read another char. */
	}

	if (buf == '\r') {
	  buf = '\n';
	  ATPUT(conn, CO_UNGETC, MKNUM(-2));	/* "eat next nl." */
	}

	break;
    }
    break;
  }

  return buf;
}

PRIVATE int nullconn_getter(void *arg) {
  return CONN_RET_ERROR;
}

#define CONN_GETS_GETC(conn) ((NUM(AT(conn, CO_TYPE)) == CONN_FILE) ? \
			      fileconn_getter(conn) : \
			      CONN_RET_ERROR)

PRIVATE int conn_gets(BVECTOR buf, int offs, int size, OVECTOR conn) {
  uint8_t *s = buf->vec + offs;
  int c;

  if (conn_closed(conn))
    return CONN_RET_ERROR;

  if (NUM(AT(conn, CO_TYPE)) != CONN_FILE)
    return CONN_RET_ERROR;

  while (size > 1) {
    c = CONN_GETS_GETC(conn);

    switch (c) {
      case CONN_RET_ERROR:
	return CONN_RET_ERROR;

      case CONN_RET_BLOCK: {
	VECTOR args = newvector_noinit(4);
	ATPUT(args, 0, (OBJ) buf);
	ATPUT(args, 1, MKNUM(offs));
	ATPUT(args, 2, MKNUM(size));
	ATPUT(args, 3, (OBJ) conn);
	block_thread(BLOCK_CTXT_READLINE, (OBJ) args);
	return CONN_RET_BLOCK;
      }

      default:
	break;
    }

    if (c == '\r') {
      int c2 = CONN_GETS_GETC(conn);
      if (c2 != '\n')
	conn_ungetc(conn, c2);
      break;
    }

    if (c == '\n')
      break;

    *s = c;
    s++;
    size--;
    offs++;
  }

  *s = '\0';
  return 0;	/* success. */
}

PRIVATE int nltrans_write(int fd, char const *buf, int len) {
  int i, j;
  char *obuf = allocmem(sizeof(char) * len * 2);

  for (i = 0, j = 0; i < len; i++) {
    if (buf[i] == '\n')
      obuf[j++] = '\r';
    obuf[j++] = buf[i];
  }

  i = write(fd, obuf, j);
  freemem(obuf);
  return i;
}

PUBLIC int conn_write(const char *buf, int size, OVECTOR conn) {
  if (NUM(AT(conn, CO_TYPE)) != CONN_FILE)
    return CONN_RET_ERROR;

  return nltrans_write(NUM(AT(conn, CO_HANDLE)), buf, size);
}

PUBLIC int conn_puts(const char *s, OVECTOR conn) {
  if (NUM(AT(conn, CO_TYPE)) != CONN_FILE)
    return CONN_RET_ERROR;

  return nltrans_write(NUM(AT(conn, CO_HANDLE)), s, strlen(s));
}

PUBLIC void conn_close(OVECTOR conn) {
  switch (NUM(AT(conn, CO_TYPE))) {
    case CONN_FILE: {
      int fd = NUM(AT(conn, CO_HANDLE));
      if (fd > 2)	/* don't close stdin, stdout, or stderr */
	close(fd);
      break;
    }

    default:
      break;
  }

  ATPUT(conn, CO_TYPE, MKNUM(CONN_NONE));
  ATPUT(conn, CO_HANDLE, NULL);
  ATPUT(conn, CO_INFO, NULL);
}

PUBLIC void fill_scaninst(SCANINST si, OVECTOR conn) {
  si->cache = -1;
  si->linenum = 0;
  si->yylval = NULL;

  switch (NUM(AT(conn, CO_TYPE))) {
    case CONN_NONE:
      si->getter = (stream_fn) nullconn_getter;
      si->arg = NULL;
      break;

    case CONN_FILE:
      si->getter = (stream_fn) fileconn_getter;
      si->arg = conn;
      break;

    case CONN_STRING:
      si->getter = (stream_fn) stringconn_getter;
      si->arg = conn;
      break;

    default:
      fprintf(stderr, "connection %p had funny type %d\n", conn, (int) NUM(AT(conn, CO_TYPE)));
      exit(MOVE_EXIT_MEMORY_ODDNESS);
  }
}

PUBLIC int conn_resume_readline(VECTOR args) {
  int retval = conn_gets((BVECTOR) AT(args, 0),
			 NUM(AT(args, 1)),
			 NUM(AT(args, 2)),
			 (OVECTOR) AT(args, 3));

  switch (retval) {
    case CONN_RET_BLOCK:
      break;

    case CONN_RET_ERROR:
      unblock_thread(current_thread);
      current_thread->vms->r->vm_acc = false;
      break;

    default:
      unblock_thread(current_thread);
      current_thread->vms->r->vm_acc = (OBJ) newstring((char *) ((BVECTOR) AT(args, 0))->vec);
      break;
  }

  return retval;
}

PUBLIC int conn_resume_accept(OVECTOR server) {
  int fd;
  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);

  fd = NUM(AT(server, CO_HANDLE));
  fd = accept(fd, (struct sockaddr *) &addr, &addrlen);

  if (fd == -1) {
    if (errno == EAGAIN) {
      block_thread(BLOCK_CTXT_ACCEPT, (OBJ) server);
      return CONN_RET_BLOCK;
    }

    unblock_thread(current_thread);
    current_thread->vms->r->vm_acc = false;
  } else {
    unblock_thread(current_thread);
    current_thread->vms->r->vm_acc = (OBJ) newfileconn(fd);
  }

  return 0;	/* success. */
}
