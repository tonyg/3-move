#ifndef Perms_H
#define Perms_H

/**************************************************************************
  Flags mean different things depending on the bearers of the flags.

  Objects:
    R flags	= can 'stat' slots, methods on this object
    W flags	= can add/remove slots, methods to/from this object
    X flags	= can access slots by name on this object
    C flag	= object can be clone()d or duplicate()d

  Slots:
    R flags	= slot's value can be read
    W flags	= slot's value can be written
    X flags	= ??
    C flag	= slot's owner can be changed in descendants

  Methods:
    R flags	= method can be captured to a closure
    W flags	= ??
    X flags	= method can be executed
    C flag	= method's can be overridden (in descendants?) by non-owner

**************************************************************************/

#define PRIVILEGEDP(x)		((x) == NULL || ((x)->flags & O_PRIVILEGED))
#define PERMS_CAN(x, mask)	(((x)->flags & O_PERMS_MASK & (mask)) == (mask))

#define O_CAN_SOMETHING(x,w,f)	(PERMS_CAN(x,(f)&O_WORLD_MASK) || \
				 ((w) == (x)->owner && PERMS_CAN(x,(f)&O_OWNER_MASK)) || \
				 (in_group(w, (x)->group) && PERMS_CAN(x,(f)&O_GROUP_MASK)) || \
				 PRIVILEGEDP(w))
#define O_CAN_R(x,w)	O_CAN_SOMETHING(x,w,O_ALL_R)
#define O_CAN_W(x,w)	O_CAN_SOMETHING(x,w,O_ALL_W)
#define O_CAN_X(x,w)	O_CAN_SOMETHING(x,w,O_ALL_X)
/* #define O_CAN_C(x,w)	((x)->owner == (w) || (x)->flags & O_C_FLAG) */

#define MS_CAN_SOMETHING(x,w,f)	((NUM(AT(x, ME_FLAGS))&(f)&O_WORLD_MASK) || \
				 ((w) == (OBJECT)AT(x, ME_OWNER) && \
				  (NUM(AT(x, ME_FLAGS))&(f)&O_OWNER_MASK)) || \
				 PRIVILEGEDP(w))
#define MS_CAN_R(x,w)	MS_CAN_SOMETHING(x,w,O_ALL_R)
#define MS_CAN_W(x,w)	MS_CAN_SOMETHING(x,w,O_ALL_W)
#define MS_CAN_X(x,w)	MS_CAN_SOMETHING(x,w,O_ALL_X)
#define MS_CAN_C(x,w)	((w) == (OBJECT)AT(x, ME_OWNER) || \
			 (NUM(AT(x, ME_FLAGS)) & O_C_FLAG) || \
			 PRIVILEGEDP(w))

extern int in_group(OBJECT what, VECTOR group);

#endif
