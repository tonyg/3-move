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
  PMAP curr = p->table[(intptr_t) obj % TABLE_SIZE];

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
  int idx = (intptr_t) obj % TABLE_SIZE;

  m->n = num;
  m->o = obj;
  m->next = p->table[idx];
  p->table[idx] = m;
}

/***********************************************************************/

PUBLIC void *start_save(FILE *dest) {
  uint32_t version = (uint32_t) htonl(DEFAULT_DBFMT);
  PDATA p = newpdata(dest);

  rewind(dest);
  fwrite(DBFMT_SIGNATURE, 1, DBFMT_SIG_LEN, dest);
  fwrite(&version, sizeof(uint32_t), 1, dest);
  p->db_version = DEFAULT_DBFMT;

  return (void *) p;
}

PUBLIC void end_save(void *handle) {
  killpdata((PDATA) handle);
}

PUBLIC void save(void *handle, OBJ root) {
  PDATA p = (PDATA) handle;
  int32_t n;
  
  if (root == NULL) {
    fputc('N', p->f);
    return;
  }

  if (TAGGEDP(root)) {
    if (NUMP(root)) {
      int32_t val = (int32_t) htonl(NUM(root));
      fputc('I', p->f);
      fwrite(&val, sizeof(int32_t), 1, p->f);
    } else if (SINGLETONP(root)) {
      uint8_t val = (uint8_t) DETAG(root);
      fputc('S', p->f);
      fwrite(&val, sizeof(uint8_t), 1, p->f);
    } else {
      fprintf(stderr, "Invalid TAGGEDP object (%p) in save of DEFAULT_DBFMT.\n", root);
      exit(MOVE_EXIT_DBFMT_ERROR);
    }
    
    return;
  }

  n = find_num(p, root);

  if (n != -1) {
    fputc('R', p->f);
    n = (int32_t) htonl(n);
    fwrite(&n, sizeof(int), 1, p->f);
    return;
  }

  n = p->next_n++;
  bind_number(p, n, root);

  fputc('O', p->f);
  n = (int32_t) htonl(n);
  fwrite(&n, sizeof(int32_t), 1, p->f);

  {
    uint32_t u_n = (uint32_t) htonl(((uint32_t) root->length << 2) | root->kind);
    fwrite(&u_n, sizeof(uint32_t), 1, p->f);
  }

  switch (root->kind) {
    case KIND_OBJECT: {
      OBJECT oroot = (OBJECT) root;
      uint32_t u = (uint32_t) htonl((oroot->flags << 1) | oroot->finalize);

      fwrite(&u, sizeof(uint32_t), 1, p->f);
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
      fwrite(((BVECTOR) root)->vec, sizeof(uint8_t), root->length, p->f);
      break;

    case KIND_OVECTOR: {
      OVECTOR oroot = (OVECTOR) root;
      int i;
      uint32_t u = (uint32_t) htonl((oroot->type << 1) | oroot->finalize);

      fwrite(&u, sizeof(uint32_t), 1, p->f);

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
  uint8_t sig[DBFMT_SIG_LEN];
  uint32_t version;

  rewind(source);
  fread(sig, 1, DBFMT_SIG_LEN, source);
  fread(&version, sizeof(uint32_t), 1, source);

  if (!memcmp(DBFMT_SIGNATURE, sig, DBFMT_SIG_LEN)) {
    p->db_version = (uint32_t) ntohl(version);
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
	  uint8_t b;
	  uint32_t u;

	  o = getmem(sizeof(Object), KIND_OBJECT, 0);
	  bind_object(p, n, o);
	  obj = (OBJECT) o;

	  fread(&b, sizeof(uint8_t), 1, p->f);
	  obj->finalize = b;
	  fread(&u, sizeof(uint32_t), 1, p->f);
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
	  fread(((BVECTOR) o)->vec, sizeof(uint8_t), hdr.length, p->f);
	  return o;

	case KIND_OVECTOR: {
	  OVECTOR obj;
	  int i;
	  uint8_t b;
	  uint32_t u;

	  fread(&b, sizeof(uint8_t), 1, p->f);
	  fread(&u, sizeof(uint32_t), 1, p->f);

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
  int32_t n;
  char t;

  t = fgetc(p->f);

  switch (t) {
    case 'N': return NULL;

    case 'I': {
      int32_t val;
      fread(&val, sizeof(int32_t), 1, p->f);
      return MKNUM((int32_t) ntohl(val));
    }

    case 'S': {
      uint8_t val;
      fread(&val, sizeof(uint8_t), 1, p->f);
      return MKSINGLETON((uintptr_t) val);
    }

    case 'R': {
      OBJ o;
      fread(&n, sizeof(int32_t), 1, p->f);
      n = (int32_t) ntohl(n);
      if (!find_obj(p, n, &o)) {
	fprintf(stderr, "error loading database! object not found by number (%d)!\n", n);
	exit(MOVE_EXIT_DBFMT_ERROR);
      }
      return o;
    } 

    case 'O': {
      uint32_t u_n;
      OBJ o;
      uint32_t u_kind, u_length;

      fread(&n, sizeof(int32_t), 1, p->f);
      n = (int32_t) ntohl(n);
      fread(&u_n, sizeof(uint32_t), 1, p->f);
      u_n = (uint32_t) ntohl(u_n);

      u_kind = u_n & 3;
      u_length = u_n >> 2;

      switch (u_kind) {
	case KIND_OBJECT: {
	  OBJECT obj;
	  uint32_t u;

	  o = getmem(sizeof(Object), KIND_OBJECT, 0);
	  bind_object(p, n, o);
	  obj = (OBJECT) o;

	  fread(&u, sizeof(uint32_t), 1, p->f);
	  u = (uint32_t) ntohl(u);
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
	  fread(((BVECTOR) o)->vec, sizeof(uint8_t), u_length, p->f);
	  return o;

	case KIND_OVECTOR: {
	  OVECTOR obj;
	  int i;
	  uint32_t u;

	  fread(&u, sizeof(uint32_t), 1, p->f);
	  u = (uint32_t) ntohl(u);
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
