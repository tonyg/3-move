#ifndef Perms_H
#define Perms_H

/**************************************************************************
  Flags mean different things depending on the bearers of the flags.

  Objects:
    R flag	= can 'stat' slots on this object
    W flag	= can add/remove slots to/from this object
    X flag	= can access slots by name on this object
    C flag	= object can be clone()d or duplicate()d

  Slots:
    R flag	= slot's value can be read
    W flag	= slot's value can be written
    X flag	= ??
    C flag	= slot's owner can be changed

  Methods:
    R flag	= method can be captured to a closure
    W flag	= ??
    X flag	= method can be executed
    C flag	= method's owner can be changed

**************************************************************************/

#define PRIVILEGEDP(x)		((x)->flags & O_PRIVILEGED)
#define PERMS_CAN(x, mask)	(((x)->flags & O_PERMS_MASK & (mask)) == (mask))

extern int in_group(OBJECT what, VECTOR group);

#endif
