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
#define DEFINING_MOVE_PRIMITIVES
#include "prim.h"
#include "buffer.h"
#include "hashtable.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define ALLOW_MUTABLE_STRINGS 0

DEFPRIM(eltRef) {
  OBJ x = ARG(0);
  OBJ i = ARG(1);

  TYPEERRIF(!((VECTORP(x) || BVECTORP(x)) && NUMP(i)) &&
	    !(OVECTORP(x) && OVECTORP(i) &&
	      ((OVECTOR)x)->type == T_HASHTABLE &&
	      ((OVECTOR)i)->type == T_SYMBOL));

  if (OVECTORP(x)) {	/* hashtable. */
    OVECTOR link;

    link = hashtable_get((OVECTOR) x, (OVECTOR) i);
    if (link == NULL)
      return undefined;
    return AT(link, US_VALUE);
  }

  if (NUM(i) < 0 || NUM(i) >= x->length) {
    vm_raise(vms, (OBJ) newsym("range-error"), (OBJ) argvec);
    return undefined;
  }

  if (VECTORP(x)) {
    return AT((VECTOR) x, NUM(i));
  } else {
    BVECTOR res = newbvector_noinit(1);

    ATPUT(res, 0, AT((BVECTOR) x, NUM(i)));
    return (OBJ) res;
  }
}

DEFPRIM(eltSet) {
  OBJ x = ARG(0);
  OBJ i = ARG(1);
  OBJ v = ARG(2);

  TYPEERRIF(!((VECTORP(x) || BVECTORP(x)) && NUMP(i)) &&
	    !(OVECTORP(x) && OVECTORP(i) &&
	      ((OVECTOR)x)->type == T_HASHTABLE &&
	      ((OVECTOR)i)->type == T_SYMBOL));

  if (OVECTORP(x)) {	/* hashtable. */
    OVECTOR link = newovector_noinit(US_MAXSLOTINDEX, T_USERHASHLINK);

    ATPUT(link, US_NAME, i);
    ATPUT(link, US_NEXT, NULL);
    ATPUT(link, US_VALUE, v);

    hashtable_put((OVECTOR) x, (OVECTOR) i, link);
    return v;
  }

  if (NUM(i) < 0 || NUM(i) >= x->length) {
    vm_raise(vms, (OBJ) newsym("range-error"), (OBJ) argvec);
    return undefined;
  }

  if (VECTORP(x))
    ATPUT((VECTOR) x, NUM(i), v);
  else {
    if (!BVECTORP(v) || v->length < 1) {
      vm_raise(vms, (OBJ) newsym("type-error"), (OBJ) argvec);
      return undefined;
    }

#if ALLOW_MUTABLE_STRINGS
    ATPUT((BVECTOR) x, NUM(i), AT((BVECTOR) v, 0));
#else
    vm_raise(vms, (OBJ) newsym("mutable-strings-disallowed"), (OBJ) argvec);
    return undefined;
#endif
  }

  return v;
}

DEFPRIM(makeVector) {
  OBJ n = ARG(0);
  OBJ v = undefined;
  VECTOR result;
  int i;

  TYPEERRIF(!NUMP(n));
  if (argvec->_.length > 2)
    v = ARG(1);

  result = newvector_noinit(NUM(n));
  for (i = 0; i < NUM(n); i++)
    ATPUT(result, i, v);

  return (OBJ) result;
}

DEFPRIM(makeString) {
  OBJ n = ARG(0);
  char v = ' ';
  BVECTOR result;
  int i;

  TYPEERRIF(!NUMP(n));
  if (argvec->_.length > 2) {
    TYPEERRIF(!BVECTORP(ARG(1)));
    v = AT((BVECTOR) ARG(1), 0);
  }

  result = newbvector_noinit(NUM(n));
  for (i = 0; i < NUM(n); i++)
    ATPUT(result, i, v);

  return (OBJ) result;
}

DEFPRIM(lengthFun) {
  OBJ x = ARG(0);

  TYPEERRIF(!BVECTORP(x) && !VECTORP(x));

  return MKNUM(x->length);
}

DEFPRIM(appendFun) {
  OBJ a = ARG(0);
  OBJ b = ARG(1);

  TYPEERRIF(!((BVECTORP(a) && BVECTORP(b)) || (VECTORP(a) && VECTORP(b))));

  if (BVECTORP(a))
    return (OBJ) bvector_concat((BVECTOR) a, (BVECTOR) b);
  else
    return (OBJ) vector_concat((VECTOR) a, (VECTOR) b);
}

DEFPRIM(deleteFun) {
  OBJ x = ARG(0);
  OBJ i = ARG(1);
  OBJ l = ARG(2);
  int index, length;

  TYPEERRIF(!(BVECTORP(x) || VECTORP(x)) || !NUMP(i) || !NUMP(l));

  index = NUM(i);
  length = NUM(l);

  if (index < 0 || length < 0 || (index + length) > x->length) {
    vm_raise(vms, (OBJ) newsym("range-error"), (OBJ) argvec);
    return undefined;
  }

  if (BVECTORP(x)) {
    BVECTOR result = newbvector_noinit(x->length - length);
    BVECTOR vx = (BVECTOR) x;
    memcpy(result->vec, vx->vec, index);
    memcpy(result->vec + index, vx->vec + index + length, result->_.length - index);
    return (OBJ) result;
  } else {
    VECTOR result = newvector_noinit(x->length - length);
    VECTOR vx = (VECTOR) x;
    int i;
    for (i = 0; i < index; i++)
      ATPUT(result, i, AT(vx, i));
    for (i = index; i < result->_.length; i++)
      ATPUT(result, i, AT(vx, i + length));
    return (OBJ) result;
  }
}

DEFPRIM(indexOfFun) {
  OBJ x = ARG(0);
  OBJ v = ARG(1);

  TYPEERRIF(!((BVECTORP(x) && BVECTORP(v)) || VECTORP(x)));

  if (BVECTORP(x)) {
    uint8_t *loc = memchr(((BVECTOR) x)->vec, AT((BVECTOR) v, 0), x->length);

    if (loc == NULL)
      return false;

    return MKNUM(loc - ((BVECTOR) x)->vec);
  } else {
    VECTOR vx = (VECTOR) x;
    int i;

    for (i = 0; i < x->length; i++)
      if (AT(vx, i) == v)
	return MKNUM(i);

    return false;
  }
}

PRIVATE uint8_t const *strSearchAux(uint8_t const *v1, int n1, uint8_t const *v2, int n2) {
  int i;

  if (n2 == 0)
    return v1;

  for (i = 0; i < (n1 - n2) + 1; i++)
    if (!strncmp((char const *) &v1[i], (char const *) v2, n2))
      return &v1[i];

  return NULL;
}

PRIVATE uint8_t const *strSearchAuxCI(uint8_t const *v1, int n1, uint8_t const *v2, int n2) {
  int i;

  if (n2 == 0)
    return v1;

  for (i = 0; i < (n1 - n2) + 1; i++)
    if (!strncasecmp((char const *) &v1[i], (char const *) v2, n2))
      return &v1[i];

  return NULL;
}

PRIVATE OBJ strSearchBody(VMSTATE vms, VECTOR argvec, int case_matters) {
  OBJ str = ARG(0);
  OBJ pat = ARG(1);
  uint8_t const *pos;

  TYPEERRIF(!BVECTORP(str) || !BVECTORP(pat));

  if (case_matters)
    pos = strSearchAux(((BVECTOR) str)->vec, str->length,
		       ((BVECTOR) pat)->vec, pat->length);
  else
    pos = strSearchAuxCI(((BVECTOR) str)->vec, str->length,
			 ((BVECTOR) pat)->vec, pat->length);

  if (pos == NULL)
    return false;
  else
    return MKNUM(pos - ((BVECTOR) str)->vec);
}

DEFPRIM(strSearch) {
  return strSearchBody(vms, argvec, 1);
}

DEFPRIM(strSearchCI) {
  return strSearchBody(vms, argvec, 0);
}

DEFPRIM(strReplace) {
  OBJ str = ARG(0);
  OBJ pat = ARG(1);
  OBJ rep = ARG(2);
  BUFFER dest = newbuf(0);
  BVECTOR result;
  uint8_t *source, *what, *with;
  int source_length, what_length, with_length;
  int si, wi;

  TYPEERRIF(!BVECTORP(str) || !BVECTORP(pat) || !BVECTORP(rep));

  source = ((BVECTOR) str)->vec;
  what = ((BVECTOR) pat)->vec;
  with = ((BVECTOR) rep)->vec;
  source_length = str->length;
  what_length = pat->length;
  with_length = rep->length;

  for (si = 0; si < source_length; si++) {
    if (!memcmp(source + si, what, what_length)) {
      si += what_length - 1;
      for (wi = 0; wi < with_length; wi++)
	buf_append(dest, with[wi]);
    } else
      buf_append(dest, source[si]);
  }

  result = newbvector_noinit(dest->pos);
  memcpy(result->vec, dest->buf, result->_.length);
  killbuf(dest);

  return (OBJ) result;
}

DEFPRIM(sectionFun) {
  OBJ x = ARG(0);
  OBJ i = ARG(1);
  OBJ l = ARG(2);
  int index, length;

  TYPEERRIF(!(BVECTORP(x) || VECTORP(x)) || !NUMP(i) || !NUMP(l));

  index = NUM(i);
  length = NUM(l);

  if (length == -1)
    length = x->length - index;

  if (index < 0 || length < 0 || (index + length) > x->length) {
    vm_raise(vms, (OBJ) newsym("range-error"), (OBJ) argvec);
    return undefined;
  }

  if (BVECTORP(x)) {
    BVECTOR result = newbvector_noinit(length);
    memcpy(result->vec, ((BVECTOR) x)->vec + index, length);
    return (OBJ) result;
  } else {
    VECTOR result = newvector_noinit(length);
    VECTOR vx = (VECTOR) x;
    int i;

    for (i = 0; i < length; i++)
      ATPUT(result, i, AT(vx, i + index));

    return (OBJ) result;
  }
}

DEFPRIM(strcmpFun) {
  OBJ a = ARG(0);
  OBJ b = ARG(1);
  int alen, blen, minlen;
  int result;

  TYPEERRIF(!BVECTORP(a) || !BVECTORP(b));

  alen = a->length;
  blen = b->length;
  minlen = alen < blen ? alen : blen;

  result = memcmp(((BVECTOR) a)->vec, ((BVECTOR) b)->vec, minlen);

  if (!result)
    return alen < blen ? MKNUM(-1) : (alen == blen) ? MKNUM(0) : MKNUM(1);

  return MKNUM(result);
}

DEFPRIM(toupperFun) {
  OBJ x = ARG(0);
  BVECTOR vx;
  int i;

  TYPEERRIF(!BVECTORP(x));

  vx = newbvector_noinit(x->length);

  for (i = 0; i < x->length; i++)
    ATPUT(vx, i, toupper(AT((BVECTOR) x, i)));

  return (OBJ) vx;
}

DEFPRIM(tolowerFun) {
  OBJ x = ARG(0);
  BVECTOR vx;
  int i;

  TYPEERRIF(!BVECTORP(x));

  vx = newbvector_noinit(x->length);

  for (i = 0; i < x->length; i++)
    ATPUT(vx, i, tolower(AT((BVECTOR) x, i)));

  return (OBJ) vx;
}

DEFPRIM(makeHTFun) {
  OBJ len = ARG(0);
  TYPEERRIF(!NUMP(len));
  return (OBJ) newhashtable(NUM(len));
}

DEFPRIM(allKeysFun) {
  OBJ h = ARG(0);
  OVECTOR ht = (OVECTOR) h;
  VECTOR result;

  TYPEERRIF(!(OVECTORP(h) && ht->type == T_HASHTABLE));

  result = enumerate_keys(ht);

  return (OBJ) result;
}

DEFPRIM(asSymFun) {
  OBJ x = ARG(0);
  BVECTOR cname;
  TYPEERRIF(!BVECTORP(x));
  cname = bvector_concat((BVECTOR) x, newbvector(1));
  return (OBJ) newsym((char *) cname->vec);
}

DEFPRIM(copyOfFun) {
  OBJ x = ARG(0);

  TYPEERRIF(!BVECTORP(x) && !VECTORP(x));

  if (BVECTORP(x)) {
    BVECTOR n = newbvector_noinit(x->length);
    memcpy(n->vec, ((BVECTOR) x)->vec, x->length);
    return (OBJ) n;
  } else {
    int i;
    VECTOR n = newvector_noinit(x->length);

    for (i = 0; i < x->length; i++)
      ATPUT(n, i, AT((VECTOR) x, i));

    return (OBJ) n;
  }
}

PUBLIC void install_PRIM_compound(void) {
  register_prim(2, "element-ref", 0x01001, eltRef);
  register_prim(3, "element-set", 0x01002, eltSet);
  register_prim(-1, "make-vector", 0x01003, makeVector);
  register_prim(-1, "make-string", 0x01004, makeString);
  register_prim(1, "length", 0x01005, lengthFun);
  register_prim(2, "append", 0x01006, appendFun);
  register_prim(3, "delete", 0x01007, deleteFun);
  register_prim(2, "index-of", 0x01008, indexOfFun);
  register_prim(2, "substring-search", 0x01009, strSearch);
  register_prim(3, "substring-replace", 0x0100A, strReplace);
  register_prim(3, "section", 0x0100B, sectionFun);
  register_prim(2, "strcmp", 0x0100C, strcmpFun);
  register_prim(1, "toupper", 0x0100D, toupperFun);
  register_prim(1, "tolower", 0x0100E, tolowerFun);
  register_prim(1, "make-hashtable", 0x0100F, makeHTFun);
  register_prim(1, "all-keys", 0x01010, allKeysFun);
  register_prim(1, "as-sym", 0x01011, asSymFun);
  register_prim(1, "copy-of", 0x01012, copyOfFun);
  register_prim(2, "substring-search-ci", 0x01013, strSearchCI);
}

