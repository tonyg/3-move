#ifndef Conn_H
#define Conn_H

extern OVECTOR newfileconn(int fd);
extern OVECTOR newstringconn(BVECTOR data);

#define CONN_RET_ERROR		-1	/* EOF or normal error-with-errno */
#define CONN_RET_BLOCK		-2	/* thread blocked, yield asap */

extern int conn_write(const char *buf, int size, OVECTOR conn);
extern int conn_puts(const char *s, OVECTOR conn);

extern void conn_close(OVECTOR conn);
#define conn_closed(conn)	((conn) == NULL || NUM(AT(conn, CO_TYPE)) == CONN_NONE)

extern void fill_scaninst(SCANINST si, OVECTOR conn);

extern int conn_resume_readline(VECTOR args);
extern int conn_resume_accept(OVECTOR server);

#endif
