#include "global.h"
#include "object.h"
#include "vm.h"
#include "prim.h"
#include "barrier.h"
#include "gc.h"
#include "primload.h"
#include "scanner.h"
#include "parser.h"
#include "conn.h"
#include "persist.h"
#include "recmutex.h"

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
  p->db_version = DBFMT_BIG_32;

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
  return (void *) newpdata(dest);
}

PUBLIC void end_save(void *handle) {
  killpdata((PDATA) handle);
}

PUBLIC void save(void *handle, OBJ root) {
  PDATA p = (PDATA) handle;
  int n;
  
  if (root == NULL) {
    fputc('N', p->f);
    return;
  }

  if (TAGGEDP(root)) {
    fputc('L', p->f);
    fwrite(&root, sizeof(OBJ), 1, p->f);
    return;
  }

  n = find_num(p, root);

  if (n != -1) {
    fputc('R', p->f);
    fwrite(&n, sizeof(int), 1, p->f);
    return;
  }

  n = p->next_n++;
  bind_number(p, n, root);

  fputc('O', p->f);
  fwrite(&n, sizeof(int), 1, p->f);
  fwrite(root, sizeof(Obj), 1, p->f);

  switch (root->kind) {
    case KIND_OBJECT: {
      OBJECT oroot = (OBJECT) root;
      byte b = oroot->finalize;
      u32 u = oroot->flags;

      fwrite(&b, sizeof(byte), 1, p->f);
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
      byte b = oroot->finalize;
      u32 u = oroot->type;

      fwrite(&b, sizeof(byte), 1, p->f);
      fwrite(&u, sizeof(u32), 1, p->f);

      if (u == T_CONNECTION) {
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
  byte sig[sizeof(DBFMT_SIGNATURE)];
  u32 version;

  rewind(source);
  fread(sig, 1, sizeof(DBFMT_SIGNATURE), source);
  fread(&version, sizeof(u32), 1, source);
  rewind(source);

  if (!memcmp(DBFMT_SIGNATURE, sig, sizeof(DBFMT_SIGNATURE))) {
    p->db_version = ntohl(version);
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
	exit(2);
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
      exit(2);
  }

  return NULL;
}

PRIVATE OBJ load_BIG_32(PDATA p) {
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
	exit(2);
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
      exit(2);
  }

  return NULL;
}

PUBLIC OBJ load(void *handle) {
  PDATA p = (PDATA) handle;

  switch (p->db_version) {
    case DBFMT_OLDFMT: return load_OLDFMT(p);
    case DBFMT_BIG_32: return load_BIG_32(p);

    default:
      fprintf(stderr, "Unrecognised database version. Sorry, I can't load that.\n");
      exit(2);
  }

  return NULL;
}
