#include "global.h"
#include "object.h"
#include "vm.h"
#include "prim.h"
#include "scanner.h"
#include "conn.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <unistd.h>
#include <fcntl.h>

#define TYPEERRIF(c)	if (c) { \
			  vm_raise(vms, (OBJ) newsym("type-error"), (OBJ) argvec); \
			  return undefined; \
			}
#define DEFPRIM(name)	PRIVATE OBJ name(VMSTATE vms, VECTOR argvec)
#define ARG(n)		AT(argvec, (n)+1)

PRIVATE OBJ getPrintString(VMSTATE, VECTOR); /* prototype */

PRIVATE BVECTOR getPrintString_body(VMSTATE vms, OBJ x, int depth) {
  char buf[80];

  if (x == NULL)
    return newstring("null");

  if (NUMP(x)) {
    sprintf(buf, "%d", NUM(x));
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

DEFPRIM(printOn) {
  OBJ conn = ARG(0);
  OBJ val = ARG(1);

  TYPEERRIF(!OVECTORP(conn) || ((OVECTOR) conn)->type != T_CONNECTION || !BVECTORP(val));

  conn_write(((BVECTOR) val)->vec, val->length, (OVECTOR) conn);
  return undefined;
}

DEFPRIM(readFrom) {
  OBJ conn = ARG(0);
  char buf[1024];

  TYPEERRIF(!OVECTORP(conn) || ((OVECTOR) conn)->type != T_CONNECTION);

  if (conn_gets(buf, 1024, (OVECTOR) conn))
    return (OBJ) newstring(buf);
  else
    return NULL;
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

  sock = socket(AF_INET, SOCK_STREAM, 0);

  if (sock == -1)
    return (OBJ) newsym("socket-creation-failed");

  memcpy(buf, ((BVECTOR) hostname)->vec, hostname->length >= 1024 ? 1023 : hostname->length);
  buf[hostname->length >= 1024 ? 1023 : hostname->length] = '\0';

  he = gethostbyname(buf);

  if (he == NULL)
    return (OBJ) newsym("host-not-found");

  addr.sin_family = AF_INET;
  (long) addr.sin_addr.s_addr = * (long *) he->h_addr;
  addr.sin_port = htons(NUM(port));

  if (connect(sock, (struct sockaddr *) &addr, sizeof(addr)) == -1)
    return (OBJ) newsym("connection-refused");

  return (OBJ) newfileconn(sock);
}

PUBLIC void installer(void) {
  register_prim("get-print-string", 0x00001, getPrintString);
  register_prim("print-on", 0x00002, printOn);
  register_prim("read-from", 0x00003, readFrom);
  register_prim("closed?", 0x00004, closedP);
  register_prim("close", 0x00005, closeConnection);
  register_prim("open", 0x00006, openConnection);
}
