#ifndef Hashtable_H
#define Hashtable_H

extern OVECTOR newhashtable(int len);

extern OVECTOR hashtable_get(OVECTOR table, OVECTOR keysym);
extern void hashtable_put(OVECTOR table, OVECTOR keysym, OVECTOR value);

#endif
