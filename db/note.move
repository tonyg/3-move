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
