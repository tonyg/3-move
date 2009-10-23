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

#include "global.h"
#include "object.h"
#include "vm.h"
#include "prim.h"
#include "gc.h"
#include "primload.h"
#include "scanner.h"
#include "parser.h"
#include "conn.h"
#include "persist.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <netinet/in.h>

#define TABLE_SIZE	6469

typedef struct PMap {
  int n;
  OBJ o;
  struct PMap *next;
} PMap, *PMAP;

typedef struct PData {
  PMAP table[TABLE_SIZE];
  int next_n;
  FILE *f;
  int db_version;
} PData, *PDATA;

PRIVATE PDATA newpdata(FILE *f) {
  PDATA p = allocmem(sizeof(PData));

  p->f = f;
  p->next_n = 0;
  memset(p->table, 0, TABLE_SIZE * sizeof(PMAP));
  p->db_version = 0;

  return p;
}

PRIVATE void killpdata(PDATA p) {
  int i;

  for (i = 0; i < TABLE_SIZE; i++) {
    PMAP c = p->table[i];

    while (c != NULL) {
      PMAP t = c->next;
      freemem(c);
      c = t;
    }
  }

  freemem(p);
}

PRIVATE int find_obj(PDATA p, int num, OBJ *result) {
  PMAP curr = p->table[num % TABLE_SIZE];

  while (curr != NULL) {
    if (curr->n == num) {
      *result = curr->o;
      return 1;
    }

    curr = curr->next;
  }

  return 0;
}

PRIVATE int find_num(PDATA p, OBJ obj) {
  PMAP curr = p->table[(int) obj % TABLE_SIZE];

  while (curr != NULL) {
    if (curr->o == obj)
      return curr->n;

    curr = curr->next;
  }

  return -1;
}

PRIVATE void bind_object(PDATA p, int num, OBJ obj) {
  PMAP m = allocmem(sizeof(PMap));
  int idx = num % TABLE_SIZE;

  m->n = num;
  m->o = obj;
  m->next = p->table[idx];
  p->table[idx] = m;
}

PRIVATE void bind_number(PDATA p, int num, OBJ obj) {
  PMAP m = allocmem(sizeof(PMap));
  int idx = (int) obj % TABLE_SIZE;

  m->n = num;
  m->o = obj;
  m->next = p->table[idx];
  p->table[idx] = m;
}

/***********************************************************************/

PUBLIC void *start_save(FILE *dest) {
  u32 version = (u32) htonl(DEFAULT_DBFMT);
  PDATA p = newpdata(dest);

  rewind(dest);
  fwrite(DBFMT_SIGNATURE, 1, DBFMT_SIG_LEN, dest);
  fwrite(&version, sizeof(u32), 1, dest);
  p->db_version = DEFAULT_DBFMT;

  return (void *) p;
}

PUBLIC void end_save(void *handle) {
  killpdata((PDATA) handle);
}

PUBLIC void save(void *handle, OBJ root) {
  PDATA p = (PDATA) handle;
  i32 n;
  
  if (root == NULL) {
    fputc('N', p->f);
    return;
  }

  if (TAGGEDP(root)) {
    if (NUMP(root)) {
      i32 val = (i32) htonl(NUM(root));
      fputc('I', p->f);
      fwrite(&val, sizeof(i32), 1, p->f);
    } else if (SINGLETONP(root)) {
      byte val = (byte) DETAG(root);
      fputc('S', p->f);
      fwrite(&val, sizeof(byte), 1, p->f);
    } else {
      fprintf(stderr, "Invalid TAGGEDP object (%p) in save of DEFAULT_DBFMT.\n", root);
      exit(MOVE_EXIT_DBFMT_ERROR);
    }
    
    return;
  }

  n = find_num(p, root);

  if (n != -1) {
    fputc('R', p->f);
    n = (i32) htonl(n);
    fwrite(&n, sizeof(int), 1, p->f);
    return;
  }

  n = p->next_n++;
  bind_number(p, n, root);

  fputc('O', p->f);
  n = (i32) htonl(n);
  fwrite(&n, sizeof(i32), 1, p->f);

  {
    u32 u_n = (u32) htonl(((u32) root->length << 2) | root->kind);
    fwrite(&u_n, sizeof(u32), 1, p->f);
  }

  switch (root->kind) {
    case KIND_OBJECT: {
      OBJECT oroot = (OBJECT) root;
      u32 u = (u32) htonl((oroot->flags << 1) | oroot->finalize);

      fwrite(&u, sizeof(u32), 1, p->f);
      save(handle, (OBJ) oroot->methods);
      save(handle, (OBJ) oroot->attributes);
      save(handle, oroot->parents);
      save(handle, (OBJ) oroot->owner);
      save(handle, (OBJ) oroot->group);
      save(handle, (OBJ) oroot->location);
      save(handle, (OBJ) oroot->contents);
      break;
    }

    case KIND_BVECTOR:
      fwrite(((BVECTOR) root)->vec, sizeof(byte), root->length, p->f);
      break;

    case KIND_OVECTOR: {
      OVECTOR oroot = (OVECTOR) root;
      int i;
      u32 u = (u32) htonl((oroot->type << 1) | oroot->finalize);

      fwrite(&u, sizeof(u32), 1, p->f);

      if (oroot->type == T_CONNECTION) {
	for (i = 0; i < CO_MAXSLOTINDEX; i++)
	  fputc('N', p->f);
      } else {
	for (i = 0; i < root->length; i++)
	  save(handle, AT(oroot, i));
      }
      break;
    }

    case KIND_VECTOR: {
      int i;

      for (i = 0; i < root->length; i++)
	save(handle, AT((VECTOR) root, i));
      break;
    }
  }
}

/***********************************************************************/

PUBLIC void *start_load(FILE *source) {
  PDATA p = newpdata(source);
  byte sig[DBFMT_SIG_LEN];
  u32 version;

  rewind(source);
  fread(sig, 1, DBFMT_SIG_LEN, source);
  fread(&version, sizeof(u32), 1, source);

  if (!memcmp(DBFMT_SIGNATURE, sig, DBFMT_SIG_LEN)) {
    p->db_version = (u32) ntohl(version);
  } else {
    rewind(source);
    p->db_version = DBFMT_OLDFMT;
  }

  return (void *) p;
}

PUBLIC void end_load(void *handle) {
  killpdata((PDATA) handle);
}

PRIVATE OBJ load_OLDFMT(PDATA p) {
  int n;
  char t;

  t = fgetc(p->f);

  switch (t) {
    case 'N': return NULL;
    case 'L': {
      OBJ o;
      fread(&o, sizeof(OBJ), 1, p->f);
      return o;
    }

    case 'R': {
      OBJ o;
      fread(&n, sizeof(int), 1, p->f);
      if (!find_obj(p, n, &o)) {
	fprintf(stderr, "error loading database! object not found by number (%d)!\n", n);
	exit(MOVE_EXIT_DBFMT_ERROR);
      }
      return o;
    } 

    case 'O': {
      Obj hdr;
      OBJ o;

      fread(&n, sizeof(int), 1, p->f);
      fread(&hdr, sizeof(Obj), 1, p->f);

      switch (hdr.kind) {
	case KIND_OBJECT: {
	  OBJECT obj;
	  byte b;
	  u32 u;

	  o = getmem(sizeof(Object), KIND_OBJECT, 0);
	  bind_object(p, n, o);
	  obj = (OBJECT) o;

	  fread(&b, sizeof(byte), 1, p->f);
	  obj->finalize = b;
	  fread(&u, sizeof(u32), 1, p->f);
	  obj->flags = u;

	  obj->methods = (OVECTOR) load_OLDFMT(p);
	  obj->attributes = (OVECTOR) load_OLDFMT(p);
	  obj->parents = load_OLDFMT(p);
	  obj->owner = (OBJECT) load_OLDFMT(p);
	  obj->group = (VECTOR) load_OLDFMT(p);
	  obj->location = (OBJECT) load_OLDFMT(p);
	  obj->contents = (VECTOR) load_OLDFMT(p);

	  recmutex_init(&(obj->lock));
	  return o;
	}
	  
	case KIND_BVECTOR:
	  o = (OBJ) newbvector(hdr.length);
	  bind_object(p, n, o);
	  fread(((BVECTOR) o)->vec, sizeof(byte), hdr.length, p->f);
	  return o;

	case KIND_OVECTOR: {
	  OVECTOR obj;
	  int i;
	  byte b;
	  u32 u;

	  fread(&b, sizeof(byte), 1, p->f);
	  fread(&u, sizeof(u32), 1, p->f);

	  obj = newovector_noinit(hdr.length, u);
	  o = (OBJ) obj;
	  bind_object(p, n, o);

	  obj->finalize = b;

	  for (i = 0; i < hdr.length; i++)
	    ATPUT(obj, i, load_OLDFMT(p));

	  return o;
	}

	case KIND_VECTOR: {
	  int i;

	  o = (OBJ) newvector_noinit(hdr.length);
	  bind_object(p, n, o);

	  for (i = 0; i < hdr.length; i++)
	    ATPUT((VECTOR) o, i, load_OLDFMT(p));

	  return o;
	}
      }
    }

    default:
      fprintf(stderr, "unknown type-char (%c) in load_OLDFMT()!\n", t);
      exit(MOVE_EXIT_DBFMT_ERROR);
  }

  return NULL;
}

PRIVATE OBJ load_NET_32(PDATA p) {
  i32 n;
  char t;

  t = fgetc(p->f);

  switch (t) {
    case 'N': return NULL;

    case 'I': {
      i32 val;
      fread(&val, sizeof(i32), 1, p->f);
      return MKNUM((i32) ntohl(val));
    }

    case 'S': {
      byte val;
      fread(&val, sizeof(byte), 1, p->f);
      return MKSINGLETON((int) val);
    }

    case 'R': {
      OBJ o;
      fread(&n, sizeof(i32), 1, p->f);
      n = (i32) ntohl(n);
      if (!find_obj(p, n, &o)) {
	fprintf(stderr, "error loading database! object not found by number (%d)!\n", n);
	exit(MOVE_EXIT_DBFMT_ERROR);
      }
      return o;
    } 

    case 'O': {
      u32 u_n;
      OBJ o;
      u32 u_kind, u_length;

      fread(&n, sizeof(i32), 1, p->f);
      n = (i32) ntohl(n);
      fread(&u_n, sizeof(u32), 1, p->f);
      u_n = (u32) ntohl(u_n);

      u_kind = u_n & 3;
      u_length = u_n >> 2;

      switch (u_kind) {
	case KIND_OBJECT: {
	  OBJECT obj;
	  u32 u;

	  o = getmem(sizeof(Object), KIND_OBJECT, 0);
	  bind_object(p, n, o);
	  obj = (OBJECT) o;

	  fread(&u, sizeof(u32), 1, p->f);
	  u = (u32) ntohl(u);
	  obj->finalize = u & 1;
	  obj->flags = u >> 1;

	  obj->methods = (OVECTOR) load_NET_32(p);
	  obj->attributes = (OVECTOR) load_NET_32(p);
	  obj->parents = load_NET_32(p);
	  obj->owner = (OBJECT) load_NET_32(p);
	  obj->group = (VECTOR) load_NET_32(p);
	  obj->location = (OBJECT) load_NET_32(p);
	  obj->contents = (VECTOR) load_NET_32(p);

	  recmutex_init(&(obj->lock));
	  return o;
	}
	  
	case KIND_BVECTOR:
	  o = (OBJ) newbvector(u_length);
	  bind_object(p, n, o);
	  fread(((BVECTOR) o)->vec, sizeof(byte), u_length, p->f);
	  return o;

	case KIND_OVECTOR: {
	  OVECTOR obj;
	  int i;
	  u32 u;

	  fread(&u, sizeof(u32), 1, p->f);
	  u = (u32) ntohl(u);
	  obj = newovector_noinit(u_length, u >> 1);
	  o = (OBJ) obj;
	  bind_object(p, n, o);

	  obj->finalize = u & 1;

	  if (obj->type == T_CONNECTION) {
	    for (i = 0; i < u_length; i++) {
	      ATPUT(obj, i, load_NET_32(p));
	      if (AT(obj, i) != NULL)
		printf("  WARNING: conn[%d] == %p\n", i, AT(obj, i));
	    }
	  } else {
	    for (i = 0; i < u_length; i++)
	      ATPUT(obj, i, load_NET_32(p));
	  }

	  return o;
	}

	case KIND_VECTOR: {
	  int i;

	  o = (OBJ) newvector_noinit(u_length);
	  bind_object(p, n, o);

	  for (i = 0; i < u_length; i++)
	    ATPUT((VECTOR) o, i, load_NET_32(p));

	  return o;
	}
      }
    }

    default:
      fprintf(stderr, "unknown type-char (%c (%d)) in load_NET_32()!\n", t, t);
      exit(MOVE_EXIT_DBFMT_ERROR);
  }

  return NULL;
}

PUBLIC OBJ load(void *handle) {
  PDATA p = (PDATA) handle;

  switch (p->db_version) {
    case DBFMT_OLDFMT: return load_OLDFMT(p);
    case DBFMT_BIG_32_OLD:
    case DBFMT_NET_32:
      return load_NET_32(p);

    default:
      fprintf(stderr, "Unrecognised database version. Sorry, I can't load that.\n");
      exit(MOVE_EXIT_DBFMT_ERROR);
  }

  return NULL;
}

PUBLIC int get_handle_dbfmt(void *handle) {
  PDATA p = (PDATA) handle;

  return p->db_version;
}
