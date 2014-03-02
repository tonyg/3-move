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
#include "gc.h"

#include "bytecode.h"
#include "buffer.h"
#include "vm.h"
#include "scanner.h"
#include "parser.h"
#include "pair.h"
#include "method.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/***************************************************************************/

typedef struct ParseInst {
  VMSTATE vms;
  SCANINST scaninst;
  int need_scan;
  int scan_result;
  int other_scan_result;
  int was_close_brace;
} ParseInst, *PARSEINST;

/***************************************************************************/

PRIVATE void stuff_token(PARSEINST p_i, int class) {
  if (!p_i->need_scan)
    p_i->other_scan_result = p_i->scan_result;

  p_i->scan_result = class;
  p_i->need_scan = 0;
}

PRIVATE int check(PARSEINST p_i, int class) {
  if (p_i->was_close_brace && class == ';') {
    p_i->was_close_brace = 0;
    stuff_token(p_i, ';');
    return 1;
  }

  if (p_i->need_scan) {
    if (p_i->other_scan_result) {
      p_i->scan_result = p_i->other_scan_result;
      p_i->other_scan_result = 0;
    } else
      p_i->scan_result = scan(p_i->scaninst);
    p_i->need_scan = 0;
  }

  return class == p_i->scan_result;
}

PRIVATE void drop(PARSEINST p_i) {
  if (p_i->need_scan) {
    if (p_i->other_scan_result) {
      p_i->scan_result = p_i->other_scan_result;
      p_i->other_scan_result = 0;
    } else
      p_i->scan_result = scan(p_i->scaninst);
  }

  p_i->was_close_brace = (p_i->scan_result == '}');
  p_i->need_scan = 1;
}

/***************************************************************************/

PUBLIC VECTOR list_to_vector(VECTOR l) {
  int len = 0, i;
  VECTOR t, n;

  for (t = l; t != NULL; t = (VECTOR) CDR(t), len++) ;
  n = newvector_noinit(len);
  for (i = 0; i < len; i++, l = (VECTOR) CDR(l))
    ATPUT(n, i, CAR(l));

  return n;
}

/***************************************************************************/

typedef struct Code {
  BUFFER buf;
  VECTOR scope;
  VECTOR littab;
  PARSEINST parseinst;
} Code, *CODE;

PRIVATE CODE newcode(PARSEINST parseinst) {
  CODE code = allocmem(sizeof(Code));

  code->buf = newbuf(0);
  code->scope = NULL;
  code->littab = NULL;
  code->parseinst = parseinst;

  return code;
}

PRIVATE void killcode(CODE code) {
  code->scope = NULL;
  code->littab = NULL;
  killbuf(code->buf);
  code->buf = NULL;

  freemem(code);	
}

PRIVATE void add_scope(CODE code) {
  VECTOR link;

  CONS(link, (OBJ) newvector_noinit(0), (OBJ) code->scope);
  code->scope = link;
}

PRIVATE void del_scope(CODE code) {
  code->scope = (VECTOR) CDR(code->scope);
}

PRIVATE VECTOR vec_append(VECTOR vec, OBJ item) {
  VECTOR n = newvector_noinit(vec->_.length + 1);
  int i;

  for (i = 0; i < vec->_.length; i++)
    ATPUT(n, i, AT(vec, i));

  ATPUT(n, vec->_.length, item);

  return n;
}

PRIVATE uint16_t add_binding(CODE code, OVECTOR name) {
  SETCAR(code->scope, (OBJ) vec_append((VECTOR) CAR(code->scope), (OBJ) name));
  return (uint16_t) ((VECTOR) CAR(code->scope))->_.length - 1;
}

PRIVATE int lookup(CODE code, OVECTOR name, uint16_t *frame, uint16_t *offset) {
  VECTOR currcell;
  uint16_t frameno;

  currcell = code->scope;
  frameno = 0;
	
  while (currcell != NULL) {
    VECTOR currframe = (VECTOR) CAR(currcell);
    uint16_t i;

    for (i = 0; i < currframe->_.length; i++)
      if (AT(currframe, i) == (OBJ) name) {
	if (frame != NULL)
	  *frame = frameno;
	if (offset != NULL)
	  *offset = i;
	return 1;
      }

    frameno++;
    currcell = (VECTOR) CDR(currcell);
  }

  return 0;
}

PRIVATE char get_lit(CODE code, OBJ lit) {
  VECTOR prev = NULL;
  VECTOR tab = code->littab;
  char index = 0;

  while (tab != NULL) {
    if (CAR(tab) == lit)
      return index;

    prev = tab;
    tab = (VECTOR) CDR(tab);
    index++;
  }

  if (prev == NULL)
    CONS(code->littab, lit, NULL);
  else {
    VECTOR link;
    CONS(link, lit, NULL);
    SETCDR(prev, (OBJ) link);
  }

  return index;
}

#define gen(c, ch)		buf_append((c)->buf, (char) (ch))
#define patch(c, o, ch)		(c)->buf->buf[(o)] = (ch)

PRIVATE void gen16(CODE c, uint16_t w) {
	gen(c, (w >> 8) & 0xFF);
	gen(c, w & 0xFF);
}

PRIVATE void patch16(CODE c, uint16_t o, uint16_t w) {
	patch(c, o, (w >> 8) & 0xFF);
	patch(c, o + 1, w & 0xFF);
}

#define JUMP_HERE_FROM(code, ip)	patch16(code, ip, CURPOS(code) - ip - 2)
#define GEN_JUMP_TO(code, b, ip)	{uint16_t __pos__;gen(code,b);__pos__=CURPOS(code);\
					     gen16(code,0);patch16(code,__pos__,ip-CURPOS(code));}

/***************************************************************************/

#define CHECK(class)		check(code->parseinst, class)
#define DROP()			drop(code->parseinst)

#define ERR(msg)		{char ERRbuf_[256]; \
				 check(code->parseinst, 0); \
				 sprintf(ERRbuf_, "%d: %s (token = %d)", \
					 code->parseinst->scaninst->linenum, \
					 msg, \
					 code->parseinst->scan_result); \
				 vm_raise(code->parseinst->vms, \
					  (OBJ) newsym("parse-error"), \
					  (OBJ) newstring(ERRbuf_)); \
				 return 0;}

#define CHKERR(class, msg)	if (!check(code->parseinst, class)) ERR(msg)

#define GEN_LIT(code, lit)	gen(code, get_lit(code, (OBJ) lit))

#define CURPOS(code)		(code)->buf->pos

#define yylval			(code->parseinst->scaninst->yylval)

#define CHKOP(opstr)		(CHECK(IDENT) && yylval == (OBJ) newsym(opstr))

#define PREPARE_CALL(code, argc)	gen(code, OP_VECTOR); \
					gen(code, (argc)+1); \
					gen(code, OP_PUSH)

/***************************************************************************/

PRIVATE int expr_parse(CODE code);	/* Prototype */

PRIVATE void compile_varref(CODE code, OVECTOR name) {
  uint16_t frame, offset;

  if (lookup(code, name, &frame, &offset)) {
    gen(code, OP_MOV_A_LOCL);
    gen(code, frame);
    gen(code, offset);
  } else {
    gen(code, OP_MOV_A_GLOB);
    GEN_LIT(code, name);
  }
}

PRIVATE int compile_methodreference(CODE code, int asmethod) {
  OVECTOR methname;

  CHKERR(IDENT, "IDENT expected after ':'");
  methname = (OVECTOR) yylval;
  DROP();

  if (CHECK('(')) { 
    int argc = 0;
    uint16_t argcloc;

    DROP();

    gen(code, OP_PUSH);	/* save away the object the method is on */

    gen(code, OP_VECTOR);
    argcloc = CURPOS(code);
    gen(code, 0);
    gen(code, OP_PUSH);

    if (CHECK(')')) {
      DROP();
    } else {
      while (1) {
	if (!expr_parse(code))
	  return 0;

	gen(code, OP_ATPUT);
	gen(code, argc + 1);
	argc++;

	if (CHECK(')')) {
	  DROP();
	  break;
	}

	CHKERR(',', "Comma separates arguments to method-call");
	DROP();
      }
    }

    patch(code, argcloc, argc + 1);

    gen(code, OP_POP);	/* A <- argvector */
    gen(code, OP_SWAP);	/* A <- pop=objectbeingcalled, push <- argvector */

    gen(code, asmethod ? OP_CALL_AS : OP_CALL);
    GEN_LIT(code, methname);
  } else {
    if (asmethod) {
      ERR("as() methods need to be called, they can't be stored");
    }

    gen(code, OP_METHOD_CLOSURE);
    GEN_LIT(code, methname);
  }

  return 1;
}

PRIVATE int applic_parse(CODE code, OVECTOR currid, int lvalue, int isslot) {
  while (1) {
    if (CHECK('(')) {
      int argc = 0;
      uint16_t argcloc;

      DROP();

      if (lvalue) {
	if (isslot) {
	  gen(code, OP_MOV_A_SLOT);
	  GEN_LIT(code, currid);
	  gen(code, OP_PUSH);
	  lvalue = 0;
	}
      } else
	gen(code, OP_PUSH);

      gen(code, OP_VECTOR);
      argcloc = CURPOS(code);
      gen(code, 0);
      gen(code, OP_PUSH);

      if (CHECK(')')) {
	DROP();
      } else {
	while (1) {
	  if (!expr_parse(code))
	    return 0;

	  gen(code, OP_ATPUT);
	  gen(code, argc + 1);
	  argc++;

	  if (CHECK(')')) {
	    DROP();
	    break;
	  }

	  CHKERR(',', "Comma separates args in funcall");
	  DROP();
	}
      }

      patch(code, argcloc, argc+1);

      if (lvalue)
	compile_varref(code, currid);
      else {
	gen(code, OP_POP);	/* A <- argvector */
	gen(code, OP_SWAP);	/* stack <- argvector, A <- topofstack==closure */
      }

      gen(code, OP_APPLY);

      lvalue = 0;
      continue;
    }

    if (CHECK('=')) {
      DROP();

      if (!lvalue) {
	ERR("lvalue required in assignment");
      }

      if (isslot)
	gen(code, OP_PUSH);

      if (!expr_parse(code))
	return 0;

      if (isslot) {
	gen(code, OP_MOV_SLOT_A);
	GEN_LIT(code, currid);
      } else {
	uint16_t frame, offset;

	if (lookup(code, currid, &frame, &offset)) {
	  gen(code, OP_MOV_LOCL_A);
	  gen(code, frame);
	  gen(code, offset);
	} else {
	  gen(code, OP_MOV_GLOB_A);
	  GEN_LIT(code, currid);
	}
      }

      break;
    }

    if (lvalue) {
      if (isslot) {
	gen(code, OP_MOV_A_SLOT);
	GEN_LIT(code, currid);
      } else {
	compile_varref(code, currid);
      }
    }

    lvalue = isslot = 0;

    if (CHECK('[')) {
      uint16_t pos;

      DROP();

      gen(code, OP_PUSH);
      gen(code, OP_VECTOR);
      pos = CURPOS(code);
      gen(code, 3);
      gen(code, OP_SWAP);
      gen(code, OP_ATPUT);
      gen(code, 1);

      if (!expr_parse(code))
	return 0;

      gen(code, OP_ATPUT);
      gen(code, 2);

      CHKERR(']', "Brackets must match in element-ref or element-set syntax");
      DROP();

      if (CHECK('=')) {
	DROP();
	patch(code, pos, 4);
	if (!expr_parse(code))
	  return 0;
	gen(code, OP_ATPUT);
	gen(code, 3);
	gen(code, OP_MOV_A_GLOB);
	GEN_LIT(code, newsym("element-set"));
	gen(code, OP_APPLY);
	break;
      } else {
	gen(code, OP_MOV_A_GLOB);
	GEN_LIT(code, newsym("element-ref"));
	gen(code, OP_APPLY);
	continue;
      }
    }

    if (CHECK('.')) {
      DROP();
      CHKERR(IDENT, "slot identifier expected after '.'");
      currid = (OVECTOR) yylval;
      DROP();
      lvalue = 1;
      isslot = 1;
      continue;
    }

    if (CHECK(':')) {
      DROP();
      if (!compile_methodreference(code, 0))
	return 0;
      continue;
    }

    break;
  }

  return 1;
}

PRIVATE int id_parse(CODE code) {
  if (CHECK('(')) {
    DROP();
    if (!expr_parse(code))
      return 0;
    CHKERR(')', "Unmatched close-paren in grouping parens");
    DROP();
    return applic_parse(code, NULL, 0, 0);
  } else if (CHECK(IDENT)) {
    OVECTOR currid = (OVECTOR) yylval;
    DROP();
    return applic_parse(code, currid, 1, 0);
  } else if (CHECK(K_AS)) {
    DROP();
    CHKERR('(', "'as' typecasts require parens");
    DROP();
    if (!expr_parse(code))
      return 0;
    CHKERR(')', "'as' typecasts require parens");
    DROP();
    CHKERR(':', "'as' typecasts are only for method reference; ':' is required");
    DROP();
    if (!compile_methodreference(code, 1))
      return 0;
    return applic_parse(code, NULL, 0, 0);
  } else if (CHECK(';')) {	/* ignore extra semicolons */
    DROP();
    return 1;
  }

  DROP();
  ERR("Syntax error in id_parse");
}

PRIVATE int constant_parse(CODE code) {
  if (CHECK(LITERAL)) {
    gen(code, OP_MOV_A_LITL);
    GEN_LIT(code, yylval);
    DROP();
    return 1;
  }

  if (CHECK(K_THIS)) {
    gen(code, OP_MOV_A_SELF);
    DROP();
    return applic_parse(code, NULL, 0, 0);
  }

  if (CHECK('[')) {
    int veclen = 0;

    DROP();

    if (CHECK(']')) {
      DROP();
      gen(code, OP_MOV_A_LITL);
      GEN_LIT(code, newvector(0));
      return applic_parse(code, NULL, 0, 0);
    }

    while (1) {
      if (!expr_parse(code))
	return 0;
      gen(code, OP_PUSH);

      veclen++;

      if (CHECK(']')) {
	DROP();
	break;
      }

      CHKERR(',', "Comma separates subexpressions in literal vector");
      DROP();
    }

    gen(code, OP_MAKE_VECTOR);
    gen(code, veclen);

    return applic_parse(code, NULL, 0, 0);
  }

  if (CHECK('!')) {
    DROP();
    if (!constant_parse(code))
      return 0;
    gen(code, OP_NOT);
    return 1;
  }

  if (CHECK('~')) {
    DROP();
    if (!constant_parse(code))
      return 0;
    gen(code, OP_BNOT);
    return 1;
  }

  if (CHECK('-')) {
    DROP();
    if (!constant_parse(code))
      return 0;
    gen(code, OP_NEG);
    return 1;
  }

  return id_parse(code);
}

PRIVATE int infix_op_parse(CODE code) {
  if (!constant_parse(code))
    return 0;

  /****************************************
  while (CHECK(IDENT)) {
    OVECTOR name = (OVECTOR) yylval;
    DROP();

    gen(code, OP_PUSH);
    PREPARE_CALL(code, 2);
    gen(code, OP_SWAP);
    gen(code, OP_ATPUT);
    gen(code, 1);

    if (!constant_parse(code))
      return 0;
    gen(code, OP_ATPUT);
    gen(code, 2);

    compile_varref(code, name);
    gen(code, OP_APPLY);
  }
  ****************************************/

  return 1;
}

PRIVATE int mul_op_parse(CODE code) {
  if (!infix_op_parse(code))
    return 0;

  while (1) {
    int op;

    if (CHKOP("*")) op = OP_STAR;
    else if (CHKOP("/")) op = OP_SLASH;
    else if (CHKOP("%")) op = OP_PERCENT;
    else break;

    DROP();
    gen(code, OP_PUSH);
    if (!infix_op_parse(code))
      return 0;
    gen(code, op);
  }

  return 1;
}

PRIVATE int add_op_parse(CODE code) {
  if (!mul_op_parse(code))
    return 0;

  while (1) {
    int op;

    if (CHKOP("+")) op = OP_PLUS;
    else if (CHECK('-')) op = OP_MINUS;
    else break;

    DROP();
    gen(code, OP_PUSH);
    if (!mul_op_parse(code))
      return 0;
    gen(code, op);
  }

  return 1;
}

PRIVATE int band_op_parse(CODE code) {
  if (!add_op_parse(code))
    return 0;

  while (1) {
    int op;

    if (CHKOP("&")) op = OP_BAND;
    else break;

    DROP();
    gen(code, OP_PUSH);
    if (!add_op_parse(code))
      return 0;
    gen(code, op);
  }

  return 1;
}

PRIVATE int bor_op_parse(CODE code) {
  if (!band_op_parse(code))
    return 0;

  while (1) {
    int op;

    if (CHKOP("|")) op = OP_BOR;
    else break;

    DROP();
    gen(code, OP_PUSH);
    if (!band_op_parse(code))
      return 0;
    gen(code, op);
  }

  return 1;
}

PRIVATE int eq_op_parse(CODE code) {
  if (!bor_op_parse(code))
    return 0;

  while (1) {
    int op;

    if (CHKOP("==")) op = OP_EQ;
    else if (CHKOP("!=")) op = OP_NE;
    else if (CHKOP(">")) op = OP_GT;
    else if (CHKOP("<")) op = OP_LT;
    else if (CHKOP(">=")) op = OP_GE;
    else if (CHKOP("<=")) op = OP_LE;
    else break;

    DROP();
    gen(code, OP_PUSH);
    if (!bor_op_parse(code))
      return 0;
    gen(code, op);
  }

  return 1;
}

PRIVATE int and_op_parse(CODE code) {
  if (!eq_op_parse(code))
    return 0;

  if (CHKOP("&&")) {
    uint16_t and_next, and_end;

    DROP();
    gen(code, OP_JUMP_TRUE);
    and_next = CURPOS(code);
    gen16(code, 0);
    gen(code, OP_JUMP);
    and_end = CURPOS(code);
    gen16(code, 0);

    JUMP_HERE_FROM(code, and_next);
    if (!and_op_parse(code))
      return 0;
    JUMP_HERE_FROM(code, and_end);
  }

  return 1;
}

PRIVATE int or_op_parse(CODE code) {
  if (!and_op_parse(code))
    return 0;

  if (CHKOP("||")) {
    uint16_t or_next, or_end;

    DROP();
    gen(code, OP_JUMP_FALSE);
    or_next = CURPOS(code);
    gen16(code, 0);
    gen(code, OP_JUMP);
    or_end = CURPOS(code);
    gen16(code, 0);

    JUMP_HERE_FROM(code, or_next);
    if (!or_op_parse(code))
      return 0;
    JUMP_HERE_FROM(code, or_end);
  }

  return 1;
}

PRIVATE int parse_id_list(CODE code) {
  int num = 0;

  if (CHECK(')')) {
    DROP();
  } else {
    while (1) {
      if (CHECK(IDENT)) {
	num++;
	add_binding(code, (OVECTOR) yylval);
	DROP();
      }

      if (CHECK(')')) {
	DROP();
	break;
      }

      if (!CHECK(',')) {
	vm_raise(code->parseinst->vms,
		 (OBJ) newsym("parse-error"),
		 (OBJ) newstring("comma reqd to separate IDENTs in id_list"));
	return -1;
      }

      DROP();
    }
  }

  return num;
}

PRIVATE OVECTOR compile_template(CODE code, int argc) {
  BUFFER obuf;
  VECTOR olittab;
  OVECTOR template;

  obuf = code->buf;
  code->buf = newbuf(0);

  olittab = code->littab;
  code->littab = NULL;

  if (!expr_parse(code)) {
    killbuf(code->buf);
    code->buf = obuf;
    code->littab = olittab;
    return NULL;
  }

  gen(code, OP_RET);

  template = newcompilertemplate(argc, (uint8_t *) code->buf->buf, code->buf->pos,
				 list_to_vector(code->littab));

  code->littab = olittab;
  killbuf(code->buf);
  code->buf = obuf;

  return template;
}

PRIVATE int expr_parse(CODE code) {
  if (CHECK(K_DEFINE)) {
    OVECTOR name;
    uint16_t frame = 0;
    uint16_t offset = 0;

    DROP();

    if (CHECK(K_METHOD)) {
      int argc;
      OVECTOR template;

      DROP();
      CHKERR('(', "'define method' requires a target enclosed in parens");
      DROP();

      PREPARE_CALL(code, 3);

      if (!expr_parse(code))
	return 0;

      CHKERR(')', "'define method' requires a target enclosed in parens");
      DROP();
 
      gen(code, OP_ATPUT);
      gen(code, 1);

      CHKERR(IDENT, "'define method' needs a method IDENT.");
      gen(code, OP_MOV_A_LITL);
      GEN_LIT(code, yylval);
      DROP();

      gen(code, OP_ATPUT);
      gen(code, 2);

      CHKERR('(', "'define method' requires formal arglist");
      DROP();

      add_scope(code);

      argc = parse_id_list(code);
      if (argc == -1)
	return 0;

      template = compile_template(code, argc);
      if (template == NULL)
	return 0;

      del_scope(code);

      gen(code, OP_MOV_A_LITL);
      GEN_LIT(code, template);
      gen(code, OP_CLOSURE);
      gen(code, OP_ATPUT);
      gen(code, 3);

      gen(code, OP_MOV_A_GLOB);
      GEN_LIT(code, newsym("add-method"));
      gen(code, OP_APPLY);

      return 1;
    }

    if (CHECK('(')) {
      DROP();

      PREPARE_CALL(code, 3);

      if (!expr_parse(code))
	return 0;

      CHKERR(')', "Definition of slots requires a target enclosed in parens");
      DROP();
 
      gen(code, OP_ATPUT);
      gen(code, 1);

      CHKERR(IDENT, "Definition of slots needs a slot IDENT.");
      gen(code, OP_MOV_A_LITL);
      GEN_LIT(code, yylval);
      DROP();

      gen(code, OP_ATPUT);
      gen(code, 2);

      if (CHECK('=')) {
	DROP();
	if (!expr_parse(code))
	  return 0;
      } else {
	gen(code, OP_MOV_A_LITL);
	GEN_LIT(code, undefined);
      }

      gen(code, OP_ATPUT);
      gen(code, 3);

      gen(code, OP_MOV_A_GLOB);
      GEN_LIT(code, newsym("add-slot"));
      gen(code, OP_APPLY);

      return 1;
    }

    if (code->scope == NULL) {
      PREPARE_CALL(code, 2);
    }

    if (CHECK(K_FUNCTION)) {
      int argc;
      OVECTOR template;

      DROP();
      CHKERR(IDENT, "IDENT reqd after 'define function'");
      name = (OVECTOR) yylval;
      DROP();
      CHKERR('(', "'define function' requires formal arglist");
      DROP();

      if (code->scope != NULL) {
	if (!lookup(code, name, &frame, &offset) || frame != 0) {
	  frame = 0;
	  offset = add_binding(code, name);
	}
      }

      add_scope(code);
      argc = parse_id_list(code);
      if (argc == -1)
	return 0;

      template = compile_template(code, argc);
      if (template == NULL)
	return 0;

      del_scope(code);

      gen(code, OP_MOV_A_LITL);
      GEN_LIT(code, template);
      gen(code, OP_CLOSURE);
    } else if (CHECK(IDENT)) {
      name = (OVECTOR) yylval;
      DROP();

      if (code->scope != NULL) {
	if (!lookup(code, name, &frame, &offset) || frame != 0) {
	  frame = 0;
	  offset = add_binding(code, name);
	}
      }

      if (CHECK('=')) {
	DROP();
	if (!expr_parse(code))
	  return 0;
      } else {
	gen(code, OP_MOV_A_LITL);
	GEN_LIT(code, undefined);
      }
    } else {
      ERR("Invalid token following define");
    }

    if (code->scope == NULL) {
      gen(code, OP_ATPUT);
      gen(code, 2);
      gen(code, OP_MOV_A_LITL);
      GEN_LIT(code, name);
      gen(code, OP_ATPUT);
      gen(code, 1);

      gen(code, OP_MOV_A_GLOB);
      GEN_LIT(code, newsym("define-global"));
      gen(code, OP_APPLY);
    } else {
      gen(code, OP_MOV_LOCL_A);
      gen(code, frame);
      gen(code, offset);
    }

    return 1;
  }

  if (CHECK(K_FUNCTION)) {
    int argc;
    OVECTOR template;

    DROP();
    CHKERR('(', "Formal arglist expected after 'function'");
    DROP();

    add_scope(code);
    argc = parse_id_list(code);
    if (argc == -1)
      return 0;

    template = compile_template(code, argc);
    if (template == NULL)
      return 0;

    del_scope(code);

    gen(code, OP_MOV_A_LITL);
    GEN_LIT(code, template);
    gen(code, OP_CLOSURE);

    return 1;
  }

  if (CHECK(K_IF)) {
    uint16_t false_jmp, end_jmp;

    DROP();

    CHKERR('(', "if needs parens around condition");
    DROP();

    if (!expr_parse(code))
      return 0;
    gen(code, OP_JUMP_FALSE);
    false_jmp = CURPOS(code);
    gen16(code, 0);

    CHKERR(')', "if needs parens around condition");
    DROP();

    if (!expr_parse(code))
      return 0;

    gen(code, OP_JUMP);
    end_jmp = CURPOS(code);
    gen16(code, 0);
    JUMP_HERE_FROM(code, false_jmp);

    if (CHECK(';')) {
      DROP();

      if (CHECK(K_ELSE)) {
	DROP();
	if (!expr_parse(code))
	  return 0;
      } else {
	stuff_token(code->parseinst, ';');
	gen(code, OP_MOV_A_LITL);
	GEN_LIT(code, undefined);
      }
    } else {
      gen(code, OP_MOV_A_LITL);
      GEN_LIT(code, undefined);
    }

    JUMP_HERE_FROM(code, end_jmp);
    return 1;
  }

  if (CHECK(K_WHILE)) {
    uint16_t loop_test, loop_end;

    DROP();

    loop_test = CURPOS(code);

    CHKERR('(', "while needs parens");
    DROP();

    if (!expr_parse(code))
      return 0;
    gen(code, OP_JUMP_FALSE);
    loop_end = CURPOS(code);
    gen16(code, 0);

    CHKERR(')', "while needs parens");
    DROP();

    if (!expr_parse(code))
      return 0;

    GEN_JUMP_TO(code, OP_JUMP, loop_test);
    JUMP_HERE_FROM(code, loop_end);

    return 1;
  }

  if (CHECK('{')) {
    uint16_t pos;

    DROP();

    if (CHECK('}')) {
      DROP();
      gen(code, OP_MOV_A_LITL);
      GEN_LIT(code, undefined);
      return 1;
    }

    add_scope(code);
    gen(code, OP_ENTER_SCOPE);
    pos = CURPOS(code);
    gen(code, 0);

    while (1) {
      if (!expr_parse(code))
	return 0;

      CHKERR(';', "Semicolon expected after each expression inside { ... }");
      DROP();

      if (CHECK('}')) {
	DROP();
	break;
      }
    }

    if (((VECTOR) CAR(code->scope))->_.length > 255) {
      ERR("Too many local variables in scope");
    }

    patch(code, pos, ((VECTOR) CAR(code->scope))->_.length);
    gen(code, OP_LEAVE_SCOPE);

    del_scope(code);
    return 1;
  }

  if (CHECK(K_RETURN)) {
    DROP();

    if (CHECK(';')) {
      gen(code, OP_MOV_A_LITL);
      GEN_LIT(code, undefined);
    } else
      if (!expr_parse(code))
	return 0;

    gen(code, OP_RET);
    return 1;
  }

  if (CHECK(K_BIND_CC)) {
    OVECTOR template;

    DROP();

    PREPARE_CALL(code, 1);

    add_scope(code);

    CHKERR('(', "brackets surround IDENT in bind-cc");
    DROP();
    CHKERR(IDENT, "IDENT required in bind-cc");
    add_binding(code, (OVECTOR) yylval);
    DROP();
    CHKERR(')', "brackets surround IDENT in bind-cc");
    DROP();

    template = compile_template(code, 1);
    if (template == NULL)
      return 0;

    del_scope(code);
    gen(code, OP_MOV_A_LITL);
    GEN_LIT(code, template);
    gen(code, OP_CLOSURE);
    gen(code, OP_ATPUT);
    gen(code, 1);

    gen(code, OP_MOV_A_GLOB);
    GEN_LIT(code, newsym("call/cc"));
    gen(code, OP_APPLY);
    return 1;
  }

  return or_op_parse(code);
}

PUBLIC OVECTOR parse(VMSTATE vms, SCANINST scaninst) {
  ParseInst p_i;
  CODE code = newcode(&p_i);
  OVECTOR template;

  p_i.vms = vms;
  p_i.scaninst = scaninst;
  p_i.need_scan = 1;
  p_i.scan_result = 0;
  p_i.other_scan_result = 0;
  p_i.was_close_brace = 0;

  if (CHECK(SCAN_EOF) || !expr_parse(code)) {
    killcode(code);
    return NULL;
  }

  if (!CHECK(';')) {
    ERR("Semi-colons must terminate expressions at toplevel")
    killcode(code);
    return NULL;
  }
  DROP();

  gen(code, OP_RET);

  template = newcompilertemplate(0, (uint8_t *) code->buf->buf, code->buf->pos,
				 list_to_vector(code->littab));
  killcode(code);
  return template;
}
