/*
 * 3-MOVE, a multi-user networked online text-based programmable virtual environment
 * Copyright 1997, 1998, 1999, 2003, 2005, 2008, 2009 Tony Garnock-Jones <tonyg@kcbbs.gen.nz>
 *
 * 3-MOVE is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * 3-MOVE is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with 3-MOVE.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef Object_H
#define Object_H

typedef struct Obj *OBJ;		/* All objects */
typedef struct Object *OBJECT;		/* Delegating objects */
typedef struct Vector *VECTOR;		/* Vectors */
typedef struct BVector *BVECTOR;	/* ByteVectors */
typedef struct OVector *OVECTOR;	/* Opaque Vectors */

typedef enum OVectorTypes {
  T_NONE = 0,

  T_HASHTABLE,
  T_SLOT,
  T_METHOD,
  T_CLOSURE,
  T_SYMBOL,
  T_PRIM,
  T_FRAME,
  T_VMREGS,
  T_CONNECTION,
  T_CONTINUATION,
  T_USERHASHLINK,

  MAX_OVECTOR_TYPE
} OVectorTypes;

#define HASHLINK_NAME	0
#define HASHLINK_NEXT	1

typedef enum SlotSlots {
  SL_NAME = 0,
  SL_NEXT,
  SL_FLAGS,		/* must be same as ME_FLAGS */
  SL_VALUE,
  SL_OWNER,

  SL_MAXSLOTINDEX
} SlotSlots;

typedef enum MethodSlots {
  ME_NAME = 0,
  ME_NEXT,
  ME_FLAGS,		/* must be same as SL_FLAGS */
  ME_CODE,
  ME_OWNER,
  ME_LITS,
  ME_ENV,
  ME_ARGC,

  ME_MAXSLOTINDEX
} MethodSlots;

typedef enum ClosureSlots {
  CL_METHOD = 0,
  CL_SELF,

  CL_MAXSLOTINDEX
} ClosureSlots;

typedef enum SymbolSlots {
  SY_NAME = 0,
  SY_NEXT,
  SY_VALUE,
  SY_HASH,

  SY_MAXSLOTINDEX
} SymbolSlots;

typedef enum PrimSlots {
  PR_NAME = 0,
  PR_NUMBER,

  PR_MAXSLOTINDEX
} PrimSlots;

typedef enum FrameSlots {
  FR_CODE = 0,			/* Code bytevector */
  FR_IP,			/* IP */
  FR_SELF,			/* Self object */
  FR_LITS,			/* Literal pool */
  FR_ENV,			/* Environment (vector-chain through elt 0) */
  FR_FRAME,			/* Link in chain */
  FR_METHOD,			/* Method for this frame */
  FR_EFFUID,			/* Effuid for this frame */

  FR_MAXSLOTINDEX
} FrameSlots;

typedef enum ConnectionSlots {
  CO_TYPE = 0,			/* kind of connection */
  CO_UNGETC,			/* buffer for conn_ungetc. */
  CO_INFO,			/* type-specific connection state */
  CO_HANDLE,			/* type-specific connection handle */

  CO_MAXSLOTINDEX
} ConnectionSlots;

typedef enum ContinuationSlots {
  CONT_FRAME = 0,
  CONT_STACK,

  CONT_MAXSLOTINDEX
} ContinuationSlots;

typedef enum UserHashLinkSlots {
  US_NAME = 0,
  US_NEXT,
  US_VALUE,

  US_MAXSLOTINDEX
} UserHashLinkSlots;

typedef struct Obj {
  OBJ next;			/* Next in all-objects-list. */
  uint32_t kind: 2;			/* 00, 01, 10, 11; kind of object */
  uint32_t marked: 1;
  uint32_t length: 29;
} Obj;

typedef struct Object {
  Obj _;

  uint32_t finalize: 1;		/* Is this instance finalizable? */
  uint32_t flags: 31;		/* Flags for this object */

  OVECTOR methods;		/* Methods hashtable. */
  OVECTOR attributes;		/* Attributes hashtable. */

  OBJ parents;			/* vector -> parents, object -> parent */
  OBJECT owner;			/* Owner */
  VECTOR group;			/* Group */

  OBJECT location;		/* Location */
  VECTOR contents;		/* Contents */

  recmutex_t lock;		/* Recursive lock over this object */
} Object;

typedef struct Vector {
  Obj _;

#ifdef __GNUC__
  OBJ vec[0];			/* Contents */
#else
  OBJ vec[1];			/* GRR no struct hack allowed */
#endif
} Vector;

typedef struct BVector {
  Obj _;

#ifdef __GNUC__
  uint8_t vec[0];			/* Contents */
#else
  uint8_t vec[1];
#endif
} BVector;

typedef struct OVector {
  Obj _;

  uint32_t finalize: 1;
  uint32_t type: 31;		/* Kind of Opaque Vector - should really be enum OVectorTypes */

#ifdef __GNUC__
  OBJ vec[0];			/* Contents */
#else
  OBJ vec[1];			/* GRR no struct hack allowed */
#endif
} OVector;

#define O_C_FLAG	0x00004000	/* general flag */
#define O_SETUID	0x00002000	/* method flag */
#define O_PRIVILEGED	0x00001000	/* object flag */
#define O_PERMS_MASK	0x00000FFF

#define O_OWNER_MASK	0x00000F00
#define O_GROUP_MASK	0x000000F0
#define O_WORLD_MASK	0x0000000F
#define O_ALL_R		0x00000111
#define O_ALL_W		0x00000222
#define O_ALL_X		0x00000444
/* #define O_ALL_C		0x00000888 */

#define O_OWNER_R	0x00000100
#define O_OWNER_W	0x00000200
#define O_OWNER_X	0x00000400
/* #define O_OWNER_C	0x00000800 */
#define O_GROUP_R	0x00000010
#define O_GROUP_W	0x00000020
#define O_GROUP_X	0x00000040
/* #define O_GROUP_C	0x00000080 */
#define O_WORLD_R	0x00000001
#define O_WORLD_W	0x00000002
#define O_WORLD_X	0x00000004
/* #define O_WORLD_C	0x00000008 */

#define CONN_NONE	0		/* this connection is closed */
#define CONN_FILE	1		/* HANDLE is fd */
#define CONN_STRING	2		/* HANDLE is contents-bvector */

#define TAG(x)		((word) (x) & 3)
#define TAGGEDP(x)	(TAG(x) != 0)
#define DETAG(x)	(((unum) (x)) >> 2)

#define NUMP(x)		(TAG(x) == 1)
#define MKNUM(x)	((OBJ) (((inum) (x) << 2) | 1))
#define NUM(x)		(((inum) (x)) >> 2)
#define UNUM(x)		(((unum) (x)) >> 2)

#define SINGLETONP(x)	(TAG(x) == 2)
#define MKSINGLETON(n)	((OBJ) (((n) << 2) | 2))

#define true		MKSINGLETON(0)
#define false		MKSINGLETON(1)
#define undefined	MKSINGLETON(2)
#define yield_thread	MKSINGLETON(3)	/* returned to VM to yield this timeslice */

#define KIND_OBJECT	0
#define KIND_BVECTOR	1
#define KIND_OVECTOR	2
#define KIND_VECTOR	3

#define OBJECTP(x)	((x) != NULL && !TAGGEDP(x) && (((OBJ) (x))->kind) == KIND_OBJECT)
#define BVECTORP(x)	((x) != NULL && !TAGGEDP(x) && (((OBJ) (x))->kind) == KIND_BVECTOR)
#define OVECTORP(x)	((x) != NULL && !TAGGEDP(x) && (((OBJ) (x))->kind) == KIND_OVECTOR)
#define VECTORP(x)	((x) != NULL && !TAGGEDP(x) && (((OBJ) (x))->kind) == KIND_VECTOR)

#define AT(x, y)	((x)->vec[(y)])
#define ATPUT(x, y, v)	(AT(x, y) = (v))

#define LOCK(o)		(recmutex_lock(&((o)->lock)))
#define UNLOCK(o)	(recmutex_unlock(&((o)->lock)))

extern VECTOR symtab;	/* for use by checkpoint_now() in vm.c
			   and the garbage collector in gc.c */

extern void init_object(void);

extern OBJECT newobject(OBJ parents, OBJECT owner);

extern VECTOR newvector(int len);
extern VECTOR newvector_noinit(int len);
extern VECTOR vector_clone(VECTOR v);

extern BVECTOR newbvector(int len);
extern BVECTOR newbvector_noinit(int len);
extern BVECTOR newstring(char *buf);

extern OVECTOR newovector(int len, int type);
extern OVECTOR newovector_noinit(int len, int type);
extern void finalize_ovector(OVECTOR o);

extern unsigned long hash_str(char *string);

extern OVECTOR newsym(char *name);

#endif
