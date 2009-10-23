// 3-MOVE, a multi-user networked online text-based programmable virtual environment
// Copyright 1997, 1998, 1999, 2003, 2005, 2008, 2009 Tony Garnock-Jones <tonyg@kcbbs.gen.nz>
//
// 3-MOVE is free software: you can redistribute it and/or modify it
// under the terms of the GNU Affero General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// 3-MOVE is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public
// License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with 3-MOVE.  If not, see <http://www.gnu.org/licenses/>.
//
// program.move
set-realuid(Wizard);
set-effuid(Wizard);

clone-if-undefined(#Program, Note);
set-object-flags(Program, O_NORMAL_FLAGS | O_C_FLAG);

Program:set-name("Generic Program");
Program:set-description(["A small piece of paper, with technical-looking marks on it.\n"]);
//Program:add-alias("program");

define (Program) target = null;
set-slot-flags(Program, #target, O_ALL_R | O_OWNER_MASK);

define method (Program) examine() {
  as(Note):examine() + "The target of this program is " + get-print-string(this.target) + ".\n";
}
make-method-overridable(Program:examine, true);

define method (Program) @target-verb(b) {
  define ptell = realuid():tell;

  if (caller-effuid() != owner(this)) {
    ptell("You cannot @target this program, because you don't own it!\n");
    return;
  }

  define tgt = b[#target][1];

  if (!tgt) {
    ptell("That target (\"" + b[#target][0] + "\") is not an object I recognise.\n");
    return;
  }

  this.target = tgt;
  ptell("You target " + this:fullname() + " at " + tgt:fullname() + ".\n");
}
Program:add-verb(#program, #@target-verb, ["@target ", #program, " at ", #target]);

define method (Program) @compile-verb(b) {
  if (effuid() != owner(this)) {
    realuid():tell("You cannot @compile this program, because you don't own it!\n");
    return;
  }

  define p = reduce(function (a, l) a + l, "", this:text());

  p = "function (TARGET) { \n" + p + "}\n";

  define thunk = compile(p);

  if (!thunk) {
    realuid():tell("There was an error during compilation.\n");
    return;
  }

  define targetable = thunk();
  define result = targetable(this.target);
  realuid():tell("Result: " + get-print-string(result) + "\n");
}
set-setuid(Program:@compile-verb, false);
Program:add-verb(#program, #@compile-verb, ["@compile ", #program]);

checkpoint();
shutdown();
.
