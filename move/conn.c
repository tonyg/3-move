#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>

#include "global.h"
#include "object.h"
#include "scanner.h"
#include "conn.h"

#if 0
#define DEBUG
#endif

PUBLIC OVECTOR newfileconn(int fd) {
  OVECTOR c = newovector(CO_MAXSLOTINDEX, T_CONNECTION);

  c->finalize = 1;
  ATPUT(c, CO_TYPE, MKNUM(CONN_FILE));
  ATPUT(c, CO_INFO, NULL);
  ATPUT(c, CO_HANDLE, MKNUM(fd));

  return c;
}

PUBLIC OVECTOR newstringconn(BVECTOR data) {
  OVECTOR c = newovector(CO_MAXSLOTINDEX, T_CONNECTION);

  c->finalize = 1;
  ATPUT(c, CO_TYPE, MKNUM(CONN_STRING));
  ATPUT(c, CO_INFO, MKNUM(0));
  ATPUT(c, CO_HANDLE, (OBJ) data);

  return c;
}

PRIVATE int stringconn_getter(OVECTOR conn) {
  BVECTOR data = (BVECTOR) AT(conn, CO_HANDLE);
  int pos = NUM(AT(conn, CO_INFO));

  if (conn_closed(conn))
    return -1;

  if (pos >= data->_.length) {
    conn_close(conn);
    return -1;
  }

  ATPUT(conn, CO_INFO, MKNUM(pos+1));
  return AT(data, pos);
}

PRIVATE int fileconn_getter(OVECTOR conn) {
  char buf;

  if (conn_closed(conn))
    return -1;

  if (read(NUM(AT(conn, CO_HANDLE)), &buf, 1) != 1) {
    conn_close(conn);
    return -1;
  }

  return buf;
}

PRIVATE int nullconn_getter(void *arg) {
  return -1;
}

PUBLIC char *conn_gets(char *s, int size, OVECTOR conn) {
  char *org = s;

  if (conn_closed(conn))
    return NULL;

  while (size > 1) {
    int c = (NUM(AT(conn, CO_TYPE)) == CONN_FILE) ? fileconn_getter(conn) : -1;

    if (c == -1) {
      return NULL;
    }

    *s = c;
    s++;
    size--;

    if (c == '\r' || c == '\n')
      break;
  }

  *s = '\0';
  return org;
}

PUBLIC int conn_write(const char *buf, int size, OVECTOR conn) {
  if (NUM(AT(conn, CO_TYPE)) != CONN_FILE)
    return -1;

  return write(NUM(AT(conn, CO_HANDLE)), buf, size);
}

PUBLIC int conn_puts(const char *s, OVECTOR conn) {
  if (NUM(AT(conn, CO_TYPE)) != CONN_FILE)
    return -1;

  return write(NUM(AT(conn, CO_HANDLE)), s, strlen(s));
}

PUBLIC void conn_close(OVECTOR conn) {
#ifdef DEBUG
  printf("(%d call close %p)\n", pthread_self(), conn);
#endif
  switch (NUM(AT(conn, CO_TYPE))) {
    case CONN_FILE: {
      int fd = NUM(AT(conn, CO_HANDLE));
#ifdef DEBUG
      printf(" closing %d ", fd); fflush(stdout);
#endif
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
      fprintf(stderr, "connection %p had funny type %d\n", conn, NUM(AT(conn, CO_TYPE)));
      exit(3);
  }
}