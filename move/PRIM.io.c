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

#include "global.h"
#include "object.h"
#include "vm.h"
#define DEFINING_MOVE_PRIMITIVES
#include "prim.h"
#include "scanner.h"
#include "conn.h"
#include "perms.h"
#include "thread.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <unistd.h>
#include <fcntl.h>

PRIVATE OBJ getPrintString(VMSTATE, VECTOR); /* prototype */

PRIVATE BVECTOR getPrintString_body(VMSTATE vms, OBJ x, int depth) {
  char buf[80];

  if (x == NULL)
    return newstring("null");

  if (NUMP(x)) {
    sprintf(buf, "%ld", (long) NUM(x));
    return newstring(buf);
  }

  if (SINGLETONP(x)) {
    if (x == true) return newstring("true");
    if (x == false) return newstring("false");
    if (x == undefined) return newstring("undefined");
    return newstring("#<unknown-singleton>");
  }

  if (OBJECTP(x))
    return newstring("#<object>");

  if (BVECTORP(x))
    return (BVECTOR) x;

  if (OVECTORP(x)) {
    OVECTOR ov = (OVECTOR) x;

    switch (ov->type) {
      case T_HASHTABLE: return newstring("#<hashtable>");
      case T_SLOT: return newstring("#<slot>");
      case T_METHOD: return newstring("#<method>");
      case T_CLOSURE: return newstring("#<closure>");
      case T_SYMBOL: return (BVECTOR) AT(ov, SY_NAME);
      case T_PRIM: return bvector_concat(newstring("#<prim "),
					 bvector_concat((BVECTOR) AT((OVECTOR) AT(ov, PR_NAME),
								     SY_NAME),
							newstring(">")));
      case T_FRAME: return newstring("#<frame>");
      case T_VMREGS: return newstring("#<vmregs>");
      case T_CONNECTION: return newstring("#<connection>");
      case T_CONTINUATION: return newstring("#<continuation>");
      case T_USERHASHLINK: return newstring("#<hashlink>");
      default: return newstring("#<unknown-ovector-type>");
    }
  }

  if (VECTORP(x)) {
    if (depth < 5) {
      VECTOR v = (VECTOR) x;
      BVECTOR result = newstring("[");
      int i;

      for (i = 0; i < (int) x->length - 1; i++) {
	result = bvector_concat(result, getPrintString_body(vms, AT(v, i), depth + 1));
	result = bvector_concat(result, newstring(", "));
      }

      if (x->length > 0)
	result = bvector_concat(result, getPrintString_body(vms, AT(v, x->length - 1), depth + 1));

      return bvector_concat(result, newstring("]"));
    } else
      return newstring("[...]");
  }

  return newstring("unhandled-type-getPrintString");
}

DEFPRIM(getPrintString) {
  return (OBJ) getPrintString_body(vms, ARG(0), 0);
}

#include <unistd.h>

DEFPRIM(printOn) {
  OBJ conn = ARG(0);
  OBJ val = ARG(1);

  TYPEERRIF(!OVECTORP(conn) || ((OVECTOR) conn)->type != T_CONNECTION || !BVECTORP(val));

  conn_write((char const *) ((BVECTOR) val)->vec, val->length, (OVECTOR) conn);
  return undefined;
}

DEFPRIM(readFrom) {
  OBJ conn = ARG(0);
  BVECTOR buf = newbvector_noinit(1024);	/* a long line! */
  VECTOR args = newvector_noinit(4);

  TYPEERRIF(!OVECTORP(conn) || ((OVECTOR) conn)->type != T_CONNECTION);

  ATPUT(args, 0, (OBJ) buf);
  ATPUT(args, 1, MKNUM(0));
  ATPUT(args, 2, MKNUM(1024));
  ATPUT(args, 3, (OBJ) conn);

  switch (conn_resume_readline(args)) {
    case CONN_RET_BLOCK:
      return yield_thread;

    case CONN_RET_ERROR:
      return false;

    default:
      return vms->r->vm_acc;	/* already set by conn_resume_readline */
  }
}

DEFPRIM(closedP) {
  OBJ conn = ARG(0);

  TYPEERRIF(!OVECTORP(conn) || ((OVECTOR) conn)->type != T_CONNECTION);

  return conn_closed((OVECTOR) conn) ? true : false;
}

DEFPRIM(closeConnection) {
  OBJ conn = ARG(0);

  TYPEERRIF(!OVECTORP(conn) || ((OVECTOR) conn)->type != T_CONNECTION);

  conn_close((OVECTOR) conn);
  return undefined;
}

DEFPRIM(openConnection) {
  OBJ hostname = ARG(0);
  OBJ port = ARG(1);
  char buf[1024];
  struct sockaddr_in addr;
  struct hostent *he;
  int sock;

  TYPEERRIF(!BVECTORP(hostname) || !NUMP(port));

  if (!PRIVILEGEDP(vms->r->vm_effuid))
    return (OBJ) newsym("permission-denied");

  sock = socket(AF_INET, SOCK_STREAM, 0);

  if (sock == -1)
    return (OBJ) newsym("socket-creation-failed");

  memcpy(buf, ((BVECTOR) hostname)->vec, hostname->length >= 1024 ? 1023 : hostname->length);
  buf[hostname->length >= 1024 ? 1023 : hostname->length] = '\0';

  he = gethostbyname(buf);

  if (he == NULL)
    return (OBJ) newsym("host-not-found");

  addr.sin_family = AF_INET;

  /* Note: this used to use (long). It didn't work on the Alpha.
     It may not work on all systems. I'm unsure as to the most portable
     solution here. */
  addr.sin_addr.s_addr = * (unsigned int *) he->h_addr;

  addr.sin_port = htons(NUM(port));

  if (connect(sock, (struct sockaddr *) &addr, sizeof(addr)) == -1)
    return (OBJ) newsym("connection-refused");

  return (OBJ) newfileconn(sock);
}

DEFPRIM(listenConnection) {
  OBJ port = ARG(0);
  struct sockaddr_in addr;
  int sock;

  TYPEERRIF(!NUMP(port));

  if (!PRIVILEGEDP(vms->r->vm_effuid))
    return (OBJ) newsym("permission-denied");

  sock = socket(AF_INET, SOCK_STREAM, 0);

  if (sock == -1)
    return (OBJ) newsym("socket-creation-failed");

  {
    int v = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v)) == -1)
      return (OBJ) newsym("setsockopt-failed");
  }

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(NUM(port));

  if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
    close(sock);
    return (OBJ) newsym("bind-failed");
  }

  if (listen(sock, 5) == -1) {
    close(sock);
    return (OBJ) newsym("listen-failed");
  }

  return (OBJ) newfileconn(sock);
}

DEFPRIM(acceptConnection) {
  OVECTOR serv = (OVECTOR) ARG(0);

  TYPEERRIF(!OVECTORP((OBJ) serv) || serv->type != T_CONNECTION);
  TYPEERRIF(conn_closed(serv) || AT(serv, CO_TYPE) != MKNUM(CONN_FILE));

  if (!PRIVILEGEDP(vms->r->vm_effuid))
    return (OBJ) newsym("permission-denied");

  switch (conn_resume_accept(serv)) {
    case CONN_RET_BLOCK:
      return yield_thread;

    case CONN_RET_ERROR:
    default:
      return vms->r->vm_acc;
  }
}

PUBLIC void install_PRIM_io(void) {
  register_prim(1, "get-print-string", 0x00001, getPrintString);
  register_prim(2, "print-on", 0x00002, printOn);
  register_prim(1, "read-from", 0x00003, readFrom);
  register_prim(1, "closed?", 0x00004, closedP);
  register_prim(1, "close", 0x00005, closeConnection);
  register_prim(2, "open", 0x00006, openConnection);
  register_prim(1, "listen", 0x00007, listenConnection);
  register_prim(1, "accept-connection", 0x00008, acceptConnection);
}
