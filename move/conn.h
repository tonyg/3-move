#ifndef Conn_H
#define Conn_H

extern OVECTOR newfileconn(int fd);
extern OVECTOR newfileconn_blocking(int fd);
extern OVECTOR newstringconn(BVECTOR data);

extern char *conn_gets(char *s, int size, OVECTOR conn);
extern int conn_write(const char *buf, int size, OVECTOR conn);
extern int conn_puts(const char *s, OVECTOR conn);
extern void conn_close(OVECTOR conn);
#define conn_closed(conn)	(NUM(AT(conn, CO_TYPE)) == CONN_NONE)

extern void fill_scaninst(SCANINST si, OVECTOR conn);

#endif
