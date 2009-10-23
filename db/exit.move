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
// exit.move
set-realuid(Wizard);
set-effuid(Wizard);

clone-if-undefined(#Exit, Thing);
set-object-flags(Exit, O_NORMAL_FLAGS | O_C_FLAG);

Exit:set-name("Generic Exit");
Exit:set-description(["Nothing special, just a door.\n"]);

define (Exit) dest = Room;
set-slot-flags(Exit, #dest, O_ALL_R | O_OWNER_MASK | O_C_FLAG);

define (Exit) opaque = false;
set-slot-flags(Exit, #opaque, O_ALL_R | O_OWNER_MASK | O_C_FLAG);

define (Exit) editable-messages = [ #look-through-msg,
				    #exit-fail-msg, #other-exit-fail-msg,
				    #entry-fail-msg, #other-entry-fail-msg,
				    #ok-msg, #other-ok-msg,
				    #other-entry-msg
				  ];

define (Exit) look-through-msg = [ "Looking through, you see...\n" ];
define (Exit) exit-fail-msg = [ "You cannot go that way.\n" ];
define (Exit) other-exit-fail-msg = [ "%N tries to exit via \"%t\", but fails.\n" ];
define (Exit) entry-fail-msg = [ "You may not enter %d.\n" ];
define (Exit) other-entry-fail-msg = [ "%N tries to enter %d, but can't get in.\n" ];

define (Exit) ok-msg = [ ];
define (Exit) other-ok-msg = [ "%n leaves, headed in the direction of %d.\n" ];
define (Exit) other-entry-msg = [ "%n enters from %u.\n" ];

define method (Exit) dig(named, source, dest) {
  define x = Exit:clone();

  x:set-name(named);
  x:set-dest(dest);
  x:space();
  source:add-exit(x);
}
set-setuid(Exit:dig, false);
make-method-overridable(Exit:dig, true);

define method (Exit) set-dest(d) {
  if (caller-effuid() != owner(this) && !privileged?(caller-effuid()))
    return false;

  this.dest = d;
  true;
}

define method (Exit) set-opaque(v) {
  if (caller-effuid() != owner(this) && !privileged?(caller-effuid()))
    return false;

  this.opaque = v;
  true;
}

define method (Exit) matches-name(name) {
  if (equal?(this.name, name))
    return true;

  define c = first-that(function (x) equal?(x, name), this:aliases());
  if (c != undefined)
    return true;

  false;
}

define method (Exit) pre-move(oldloc, newloc) false;
make-method-overridable(Exit:pre-move, true);

define method (Exit) transport(item) {
  define oldloc = location(item);
  define caller = caller-effuid();

  define function exit-sub(vec)
    map(function (x) {
      x = pronoun-sub(x, item);
      x = substring-replace(x, "%d", this.dest.name);
      x = substring-replace(x, "%t", this.name);
      substring-replace(x, "%u", oldloc.name);
    }, vec);

  if (this:locked-for?(item)) {
    item:mtell(exit-sub(this.exit-fail-msg));
    oldloc:announce-except(item, exit-sub(this.other-exit-fail-msg));
    return;
  }

  define function do-the-move() {
    set-effuid(caller);
    item:moveto(this.dest);
  }

  if (do-the-move()) {
    item:mtell(exit-sub(this.ok-msg));
    oldloc:announce-except(item, exit-sub(this.other-ok-msg));
    location(item):announce-except(item, exit-sub(this.other-entry-msg));
  } else {
    item:mtell(exit-sub(this.entry-fail-msg));
    oldloc:announce-except(item, exit-sub(this.other-entry-fail-msg));
  }
}

/////////////////////////////////////////////////////////////
// VERBS

define method (Exit) look() {
  this:description() + (if (this.opaque)
		          ["You can't see through.\n"];
			else
		          ["\n"] + this.look-through-msg + this.dest:look());
}
make-method-overridable(Exit:look, true);

define method (Exit) examine()
  as(Thing):examine() + "The exit leads to " + this.dest:fullname() + ".\n";
make-method-overridable(Exit:examine, true);

define method (Exit) @editmsg-verb(b) {
  define msgname = as-sym(b[#msgname][0]);

  if (caller-effuid() != owner(this) && !privileged?(caller-effuid())) {
    realuid():tell("Permission denied editing message.\n");
    return;
  }

  if (index-of(this.editable-messages, msgname))
    slot-set(this, msgname, editor(slot-ref(this, msgname)));
  else
    realuid():tell("That isn't an editable message.\n");
}
make-method-overridable(Exit:@editmsg-verb, true);
Exit:add-verb(#exit, #@editmsg-verb, ["@editmsg ", #msgname, " of ", #exit]);

define method (Exit) go-verb(b) {
  this:transport(realuid());
}
set-setuid(Exit:go-verb, false);
Exit:add-verb(#exit, #go-verb, ["go ", #exit]);

checkpoint();
shutdown();
.
