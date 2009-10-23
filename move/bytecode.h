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

#ifndef Bytecode_H
#define Bytecode_H

/* All quantities larger than 8 bits are stored BIG-ENDIAN. */

/* A accumulator
 * E env
 * L lits
 * S self
 * F frame
 * stack
 */

#define OP_AT			0x00	/* OP index */		/* A <- A[i] */
#define OP_ATPUT		0x01	/* OP index */		/* stacktop[i] <- A */
								/* NOTE: no pop! */

#define OP_MOV_A_LOCL		0x10	/* OP scope offset */	/* A <- local var */
#define OP_MOV_A_GLOB		0x11	/* OP namelitindex */	/* A <- global var */
#define OP_MOV_A_SLOT		0x12	/* OP namelitindex */	/* A <- named slot of A */
#define OP_MOV_A_LITL		0x13	/* OP litindex */	/* A <- L[index] */
#define OP_MOV_A_SELF		0x14	/* OP */		/* A <- S */
#define OP_MOV_A_FRAM		0x15	/* OP */		/* A <- F */
#define OP_MOV_LOCL_A		0x16	/* OP scope offset */	/* local var <- A */
#define OP_MOV_GLOB_A		0x17	/* OP namelitindex */	/* global var <- A */
#define OP_MOV_SLOT_A		0x18	/* OP namelitindex */	/* named slot of pop <- A */
#define OP_MOV_FRAM_A		0x19	/* OP */		/* F <- A */
#define OP_PUSH			0x1A	/* OP */		/* stack <-... A */
#define OP_POP			0x1B	/* OP */		/* A <-... stack */
#define OP_SWAP			0x1C	/* OP */		/* A <-... stack AND
								   stack <-... A */

#define OP_VECTOR      		0x20	/* OP length */		/* A <- new vector */
#define OP_ENTER_SCOPE		0x21	/* OP numentries */	/* E <- new scope on E */
#define OP_LEAVE_SCOPE		0x22	/* OP */		/* E <- old scope from E */
#define OP_MAKE_VECTOR		0x23	/* val1 val2... OP n */	/* A <- n-elt vector fm stack */

/*	OP_FRAME		0x30	disused as of 2-move. */
#define OP_CLOSURE		0x31	/* OP */		/* A <- closed template from A */
#define OP_METHOD_CLOSURE	0x32	/* OP namelitindex */	/* A <- closure A:name */
#define OP_RET			0x33	/* OP */		/* restore VM <- F */
#define OP_CALL			0x34	/* OP namelitindex */	/* calls method on A so named */
								/* with args in vector at pop */
#define OP_CALL_AS		0x35	/* OP namelitindex */	/* calls method on A so named */
								/* with self as S and args */
								/* in vector at pop */
#define OP_APPLY		0x36	/* OP */		/* applies closure in A to */
								/* args in vector at pop */

#define OP_JUMP			0x40	/* OP offset16 */	/* ip <- ip + offset */
#define OP_JUMP_TRUE		0x41	/* OP offset16 */	/* jumps if A nonfalse */
#define OP_JUMP_FALSE		0x42	/* OP offset16 */	/* jumps if A false */

#define OP_NOT			0x50	/* OP */		/* A <- !A */
#define OP_EQ			0x51	/* OP */		/* A <- A == pop */
#define OP_NE			0x52	/* OP */		/* A <- A != pop */
#define OP_GT			0x53	/* OP */		/* A <- pop > A */
#define OP_LT			0x54	/* OP */		/* A <- pop < A */
#define OP_GE			0x55	/* OP */		/* A <- pop >= A */
#define OP_LE			0x56	/* OP */		/* A <- pop <= A */

#define OP_NEG			0x60	/* OP */		/* A <- -A */
#define OP_BNOT			0x61	/* OP */		/* A <- ~A */
#define OP_BOR			0x62	/* OP */		/* A <- pop|A */
#define OP_BAND			0x63	/* OP */		/* A <- pop&A */
#define OP_PLUS			0x64	/* OP */		/* A <- pop+A */
#define OP_MINUS		0x65    /* OP */		/* A <- pop-A */
#define OP_STAR			0x66    /* OP */		/* A <- pop*A */
#define OP_SLASH		0x67    /* OP */		/* A <- pop/A */
#define OP_PERCENT		0x68    /* OP */		/* A <- pop%A */

#endif
