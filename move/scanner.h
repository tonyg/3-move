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
