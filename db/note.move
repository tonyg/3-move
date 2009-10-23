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
// note.move
set-realuid(Wizard);
set-effuid(Wizard);

clone-if-undefined(#Note, Thing);
set-object-flags(Note, O_NORMAL_FLAGS | O_C_FLAG);

Note:set-name("Generic Note");
Note:set-description(["A small piece of lined paper.\n"]);
//Note:add-alias("note");

define (Note) text = [];
set-slot-flags(Note, #text, O_OWNER_MASK);

define method (Note) text() {
  if (this:locked-for?(caller-effuid()))
    false;
  else
    copy-of(this.text);
}

define method (Note) set-text(t) {
  if (caller-effuid() != owner(this) && !privileged?(caller-effuid()))
    return false;

  this.text = t;
  true;
}

define method (Note) description() {
  as(Thing):description() + [(if (length(this.text) == 0)
			        "It is blank.\n";
			      else
			        "It seems to have some writing on it.\n")];
}
make-method-overridable(Note:description, true);

define method (Note) read-verb(b) {
  define ptell = realuid():tell;
  define txt = this:text();

  if (!txt)
    ptell("It seems to be written in some kind of code which you can't understand.\n");
  else {
    ptell(this.name + ":\n");
    realuid():mtell(txt);
    ptell("\n(You finish reading.)\n");
  }
}
set-setuid(Note:read-verb, false);
Note:add-verb(#note, #read-verb, ["read ", #note]);

define method (Note) edit-verb(b) {
  define ptell = realuid():tell;
  define txt = this:text();

  if (!txt) {
    ptell("You can't read the text on the note, and when you try to write on it,\n"
	  "the marks you make don't stick.\n");
  } else {
    define v = editor(txt);
    if (v == txt)
      ptell("Edit cancelled.\n");
    else if (this:set-text(v))
      ptell("Edit successful.\n");
    else
      ptell("You could not save your changes.\n");
  }
}
set-setuid(Note:edit-verb, false);
Note:add-verb(#note, #edit-verb, ["edit ", #note]);
Note:add-verb(#note, #edit-verb, ["write ", #note]);

checkpoint();
shutdown();
.
