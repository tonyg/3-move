#ifndef Method_H
#define Method_H

extern OVECTOR newcompilertemplate(int argc, byte *code, int codelen, VECTOR littab);

extern void addmethod(OBJECT obj, OVECTOR namesym, OBJECT owner, OVECTOR template);
extern OVECTOR findmethod(OBJECT obj, OVECTOR namesym);

#endif
