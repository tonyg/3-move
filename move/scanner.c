#include "global.h"
#include "object.h"
#include "buffer.h"
#include "scanner.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

PRIVATE int scan_peek(SCANINST si) {
  if (si->cache == -1) {
    si->cache = si->getter(si->arg);
  }

  if (si->cache == '\r')
    si->cache = '\n';

  if (si->cache == '\n')
    si->linenum++;

  return si->cache;
}

PRIVATE void scan_drop(SCANINST si) {
  if (si->cache == -1)
    si->getter(si->arg);

  si->cache = -1;
}

PRIVATE void scan_insert(BUFFER buf, int ch) {
  buf_append(buf, ch);
}

PRIVATE void scan_shift(SCANINST si, BUFFER buf) {
  int ch = scan_peek(si);

  if (ch == EOF)
    return;

  scan_insert(buf, ch);
  scan_drop(si);
}

PRIVATE char *compile_string_ext_rep(char *str) {
  char *src = str;
  char *dest = str;
	
  while (*src != '\0') {
    if (*src == '\\') {
      src++;
      switch (*src) {
	case 'r': *dest = '\r'; break;
	case 'n': *dest = '\n'; break;
	case 't': *dest = '\t'; break;
	case 'b': *dest = '\b'; break;
	case 'a': *dest = '\a'; break;
	case 'e': *dest = 27; break;	/* backslash-E for escape. (for ANSI etc...) */
	default: *dest = *src; break;
      }
    } else
      *dest = *src;
    src++;
    dest++;
  }

  *dest = '\0';

  return str;
}

PRIVATE struct {
  char *name;
  int k;
} scan_keywords[] = {
  { "define", K_DEFINE },
  { "method", K_METHOD },
  { "if", K_IF },
  { "else", K_ELSE },
  { "while", K_WHILE },
  { "do", K_DO },
  { "return", K_RETURN },
  { "function", K_FUNCTION },
  { "bind-cc", K_BIND_CC },
  { "as", K_AS },
  { "this", K_THIS },

  { NULL, 0 }
};

PRIVATE int keyword(char *str) {
  int i;

  i = 0;

  while (scan_keywords[i].name != NULL) {
    if (!strcmp(str, scan_keywords[i].name))
      return scan_keywords[i].k;

    i++;
  }

  return -1;
}

#define SCAN_PEEK()	scan_peek(scaninst)
#define SCAN_DROP()	scan_drop(scaninst)
#define SCAN_INSERT(ch)	scan_insert(buf, ch)
#define SCAN_SHIFT()	scan_shift(scaninst, buf)
#define yylval		scaninst->yylval

#define ST_START	0
#define ST_ID		1
#define ST_NUMBER	2
#define ST_STRING	3

#define GO(st)		{ state = st; continue; }
#define EMIT(v)		{ int __rval__ = (v); killbuf(buf); return __rval__; }

PRIVATE char scan_delims[] = "!~;:.,'#[](){}\"";

PUBLIC int scan(SCANINST scaninst) {
  int state = ST_START;
  int ch;
  BUFFER buf;
  long num = 0;
  int radix = 10;
  int sign = 1;
  int quoted = 0;

  buf = newbuf(0);

  while (1) {
    switch (state) {
      case ST_START:
        num = 0;
	sign = 1;
	radix = 10;
	quoted = 0;

        ch = SCAN_PEEK();

	if (ch == EOF)
	  EMIT(SCAN_EOF);

	if (isspace(ch)) {
	  SCAN_DROP();
	  GO(ST_START);
	}

	if (isdigit(ch))
	  GO(ST_NUMBER);

	switch (ch) {
	  case '/':
	    SCAN_DROP();

	    if (SCAN_PEEK() == '/') {
	      do
		SCAN_DROP();
	      while (SCAN_PEEK() != '\n');
	      GO(ST_START)
	    } else {
	      SCAN_INSERT('/');
	      GO(ST_ID)
	    }

	  case '-':
	    SCAN_DROP();
	    sign = -1;

	    if (isdigit(SCAN_PEEK())) {
	      GO(ST_NUMBER)
	    } else {
	      EMIT('-')
	    }

	  case '"':
	    SCAN_DROP();
	    GO(ST_STRING)

	  case '#':
	    SCAN_DROP();
	    quoted = 1;
	    GO(ST_ID)

	  case '!':
	    SCAN_DROP();
	    if (SCAN_PEEK() == '=') {
	      SCAN_INSERT('!');
	      GO(ST_ID)
	    } else
	      EMIT('!')

	  default:
	    break;
	}

	if (strchr(scan_delims, ch)) {
	  SCAN_DROP();
	  EMIT(ch)
	}

	GO(ST_ID)

      case ST_ID: {
	char *str;

	ch = SCAN_PEEK();

	if (ch == EOF)
	  EMIT(SCAN_EOF);

	if (!isspace(ch) && (strchr(scan_delims, ch) == NULL)) {
	  SCAN_SHIFT();
	  GO(ST_ID)
	}

	SCAN_INSERT('\0');
	str = buf->buf;

	if (!strcmp(str, "="))
	  EMIT('=');

	if (!strcmp(str, "null")) {
	  yylval = NULL;
	  EMIT(LITERAL)
	}

	if (!strcmp(str, "undefined")) {
	  yylval = undefined;
	  EMIT(LITERAL)
	}

	if (!strcmp(str, "true")) {
	  yylval = true;
	  EMIT(LITERAL)
	}

	if (!strcmp(str, "false")) {
	  yylval = false;
	  EMIT(LITERAL)
	}

	if (quoted) {
	  yylval = (OBJ) newsym(str);
	  EMIT(LITERAL)
	} else {
	  int k = keyword(str);

	  if (k == -1) {
	    yylval = (OBJ) newsym(str);
	    EMIT(IDENT)
	  } else
	    EMIT(k)
	}
      }

      case ST_NUMBER:
	ch = toupper(SCAN_PEEK());

	if (isdigit(ch) || (ch >= 'A' && ch <= 'F')) {
	  SCAN_DROP();
	  num *= radix;
	  if (ch >= 'A')
	    num += ch - 'A' + 10;
	  else
	    num += ch - '0';
	  GO(ST_NUMBER)
	}

	if (ch == 'X') {
	  SCAN_DROP();
	  radix = 16;
	  GO(ST_NUMBER)
	}

	num *= sign;
	yylval = MKNUM(num);
	EMIT(LITERAL)

      case ST_STRING:
	ch = SCAN_PEEK();

	switch (ch) {
	  case EOF:
	    EMIT(SCAN_EOF)

	  case '\\':
	    SCAN_DROP();
	    if (SCAN_PEEK() != '"') {
	      SCAN_INSERT('\\');
	    } else {
	      SCAN_SHIFT();
	    }
	    GO(ST_STRING)

	  case '"':
	    SCAN_DROP();
	    while (isspace(ch = SCAN_PEEK()))
	      SCAN_DROP();
	    if (ch == '"') {
	      SCAN_DROP();
	      GO(ST_STRING)
	    }
	    SCAN_INSERT('\0');
	    yylval = (OBJ) newstring(compile_string_ext_rep(buf->buf));
	    EMIT(LITERAL)

	  default:
	    SCAN_SHIFT();
	    GO(ST_STRING)
	}

      default:
	break;
    }
  }
}
