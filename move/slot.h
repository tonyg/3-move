#ifndef Slot_H
#define Slot_H

extern OVECTOR addslot(OBJECT obj, OVECTOR namesym, OBJECT owner);
extern OVECTOR findslot(OBJECT obj, OVECTOR namesym, OBJECT *foundin);
extern void delslot(OBJECT obj, OVECTOR namesym);

#endif
