#ifndef Buffer_H
#define Buffer_H

typedef struct Buffer {
    int buflength;
    int pos;
    char *buf;
} Buffer, *BUFFER;

extern BUFFER newbuf(int initial_length);
extern void killbuf(BUFFER buf);
extern void buf_append(BUFFER buf, char ch);

#endif
