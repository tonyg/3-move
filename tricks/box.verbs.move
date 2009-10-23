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
// TARGET this program to Generic Container.

set-object-flags(TARGET, O_NORMAL_FLAGS | O_C_FLAG);

define (TARGET) open = false;
set-slot-flags(TARGET, #open, O_OWNER_MASK | O_ALL_R);

define (TARGET) noisy = true;
set-slot-flags(TARGET, #noisy, O_OWNER_MASK | O_ALL_R);

define method (TARGET) pre-accept(thing, from) true;
make-method-overridable(TARGET:pre-accept, true);

define method (TARGET) open(openstate) {
  define p = caller-effuid();

  if (this:locked-for?(p) || (p != owner(this) && !privileged?(p)))
    return false;

  this.open = openstate;
  true;
}
make-method-overridable(TARGET:open, true);

define method (TARGET) make-noisy(state) {
  if (caller-effuid() != owner(this) && !privileged?(caller-effuid()))
    return false;

  this.noisy = val;
  true;
}
make-method-overridable(TARGET:make-noisy, true);

define method (TARGET) look() {
  define o = contents(this);

  if (this.open) {
    if (length(o) > 0)
      as(Thing):look() + wrap-string("It contains " + make-sentence(o) + ".\n", 0);
    else
      as(Thing):look() + ["It is empty.\n"];
  } else
    as(Thing):look();
}
make-method-overridable(TARGET:look, true);

define method (TARGET) put-verb(b) {
  define p = realuid();
  define oname = b[#obj][0];
  define o = b[#obj][1];

  if (!this.open) {
    p:tell("It's not open.\n");
    return;
  }

  if (o) {
    define oldloc = location(o);

    if (oldloc == this) {
      p:tell("It's already there.\n");
    } else if (o:moveto(this) == true) {
      p:mtell(["You put ", o.name, " in ", this.name, ".\n"]);
      if (this.noisy)
	oldloc:announce([p.name, " puts ", o.name, " in ", this.name, ".\n"]);
    } else {
      p:mtell(["You fail to put ", o.name, " in ", this.name, ".\n"]);
      if (this.noisy)
	oldloc:announce([p.name, " tries to put ", o.name, " in ", this.name, ", but fails.\n"]);
    }
  } else
    p:tell("You can't see anything called \"" + oname + "\" here.\n");
}
set-setuid(TARGET:put-verb, false);
make-method-overridable(TARGET:put-verb, true);
for-each(function (pattern) TARGET:add-verb(#box, #put-verb, pattern),
	 [
	  ["put ", #obj, " in ", #box],
	  ["place ", #obj, " in ", #box]
	 ]);

define method (TARGET) get-verb(b) {
  define p = realuid();
  define oname = b[#obj][0];
  define o = b[#obj][1];

  if (!this.open) {
    p:tell("It's not open.\n");
    return;
  }

  if (!o)
    o = this:match-object(oname);

  if (o) {
    define oldloc = location(o);

    if (oldloc != this) {
      p:tell("It's not in there.\n");
    } else if (o:moveto(p) == true) {
      p:mtell(["You take ", o.name, " out of ", this.name, ".\n"]);
      if (this.noisy)
	oldloc:announce([p.name, " takes ", o.name, " out of ", this.name, ".\n"]);
    } else {
      p:mtell(["You fail to take ", o.name, " out of ", this.name, ".\n"]);
      if (this.noisy)
	oldloc:announce([p.name, " tries to take ", o.name, " out of ", this.name, ", but fails.\n"]);
    }
  } else
    p:tell("You can't see anything called \"" + oname + "\" here.\n");
}
set-setuid(TARGET:get-verb, false);
make-method-overridable(TARGET:get-verb, true);
for-each(function (pattern) TARGET:add-verb(#box, #get-verb, pattern),
	 [
	  ["take ", #obj, " from ", #box],
	  ["take ", #obj, " out of ", #box],
	  ["get ", #obj, " from ", #box],
	  ["remove ", #obj, " from ", #box]
	 ]);

define method (TARGET) open-verb(b) {
  if (this:open(true))
    realuid():tell("You open " + this.name + ".\n");
  else
    realuid():tell("You can't open " + this.name + ".\n");
}
set-setuid(TARGET:open-verb, false);
TARGET:add-verb(#box, #open-verb, ["open ", #box]);

define method (TARGET) close-verb(b) {
  if (this:open(false))
    realuid():tell("You close " + this.name + ".\n");
  else
    realuid():tell("You can't close " + this.name + ".\n");
}
set-setuid(TARGET:close-verb, false);
TARGET:add-verb(#box, #close-verb, ["close ", #box]);

define method (TARGET) @noisy-verb(b) {
  this:make-noisy(true);
}
set-setuid(TARGET:@noisy-verb, false);
TARGET:add-verb(#box, #@noisy-verb, ["@noisy ", #box]);

define method (TARGET) @quiet-verb(b) {
  this:make-noisy(false);
}
set-setuid(TARGET:@quiet-verb, false);
TARGET:add-verb(#box, #@quiet-verb, ["@quiet ", #box]);
