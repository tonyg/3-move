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

