// thing.move
set-realuid(Wizard);
set-effuid(Wizard);

clone-if-undefined(#Thing, Root);
set-object-flags(Thing, O_NORMAL_FLAGS | O_C_FLAG);

Thing:set-name("Generic Thing");
move-to(Thing, null);

define (Thing) description = ["A non-descript kind of thing, really. Not much to look at.\n"];
set-slot-flags(Thing, #description, O_OWNER_MASK);

define (Thing) aliases = [];
set-slot-flags(Thing, #aliases, O_OWNER_MASK);

define (Thing) verbs = [];
set-slot-flags(Thing, #verbs, O_OWNER_MASK);

define method (Thing) initialize() {
  as(Root):initialize();
  this.verbs = [];
}
make-method-overridable(Thing:initialize, true);

define method (Thing) add-alias(n) {
  if (caller-effuid() != owner(this) && !privileged?(caller-effuid()))
    return false;

  if (slot-clear?(this, #aliases))
    this.aliases = [n];
  else
    this.aliases = this.aliases + [n];
  return true;
}

define method (Thing) aliases() {
  (if (slot-clear?(this, #aliases)) []; else this.aliases) +
  (if (this == Thing) []; else
    reduce(function (acc, par) acc + par:aliases(), [], parents(this)));
}

define method (Thing) add-verb(selfvar, methname, pattern) {
  define c = caller-effuid();
  define fl = object-flags(this);

  if ((fl & O_WORLD_W == O_WORLD_W) ||
      ((fl & O_GROUP_W == O_GROUP_W) && in-group-of(c, this)) ||
      ((fl & O_OWNER_W == O_OWNER_W) && c == owner(this)) ||
      privileged?(c)) {
    define vb = first-that(function (v) {
      v[2] == methname && v[1] == selfvar && vector-equal?(v[0], pattern);
    }, this.verbs);
    if (vb == undefined)
      this.verbs = this.verbs + [[pattern, selfvar, methname]];
    else {
      vb[0] = pattern;
    }
    true;
  } else
    false;
}

define method (Thing) match-verb(sent) {
  define v = this.verbs;
  define i = 0;

  while (i < length(v)) {
    define pat = v[i][0];
    define selfvar = v[i][1];
    define methname = v[i][2];
    define patlen = length(pat);
    define bindings = make-hashtable(17);

    define function accumulate-matches(rsent, idx) {
      if (idx >= patlen)
	return length(rsent) == 0;

      define thiselt = pat[idx];

      if (type-of(thiselt) == #symbol) {
	if (idx + 1 == patlen) {
	  bindings[thiselt] = rsent;
	  return true;
	}

	define nextelt = pat[idx + 1];

	if (type-of(nextelt) != #string)
	  return false;

	define nel = length(nextelt);
	define loc = substring-search-ci(rsent, nextelt);

	if (!loc)
	  return false;

	bindings[thiselt] = section(rsent, 0, loc);
	return accumulate-matches(section(rsent, loc + nel, length(rsent) - (loc + nel)), idx + 2);
      } else if (type-of(thiselt) == #string) {
	if (substring-search-ci(rsent, thiselt) != 0)
	  return false;

	define tel = length(thiselt);
	return accumulate-matches(section(rsent, tel, length(rsent) - tel), idx + 1);
      }
    }

    define ccretval = call/cc(function (cc) {
      if (accumulate-matches(sent, 0)) {
	bindings[#this] = [false, this];
	cc([selfvar, methname, bindings, cc]);
      } else
	cc(#continue);
    });

    if (ccretval != #continue)
      return ccretval;

    i = i + 1;
  }

  define function search-vec(vec) {
    define i = 0;

    while (i < length(vec)) {
      define result = vec[i]:match-verb(sent);
      if (result) {
	result[2][#this] = [false, this];
	return result;
      }
      i = i + 1;
    }

    false;
  }

  (this != Thing && search-vec(parents(this)));
}

define method (Thing) match-objects(match) {
  define bindings = match[2];
  define keys = all-keys(bindings);
  define i = 0;

  while (i < length(keys)) {
    define key = keys[i];
    define str = bindings[key];

    if (type-of(str) == #string) {	// else it's already been matched.
      define match-result = this:match-object(str);
      if (match-result)
	bindings[key] = [str, this:match-object(str)];
    }

    i = i + 1;
  }
}

define method (Thing) match-object(name) {
  if (equal?(name, this.name))
    return this;

  define c = first-that(function (x) equal?(x, name), this:aliases());
  if (c != undefined)
    return this;

  c = find-by-name(contents(this), name);
  if (c != undefined)
    return c;

  c = first-that(function (o) {
		   first-that(function (x) equal?(x, name), o:aliases()) != undefined;
		 }, contents(this));
  if (c != undefined)
    return c;

  return false;
}
make-method-overridable(Thing:match-object, true);

define method (Thing) read() "";
make-method-overridable(Thing:read, true);

define method (Thing) mtell(vec) {
  if (type-of(vec) == #vector)
    for-each(this:tell, vec);
  else
    this:tell(vec);
}

define method (Thing) tell(x) undefined;
make-method-overridable(Thing:tell, true);

define method (Thing) listening?() false;
make-method-overridable(Thing:listening?, true);

define method (Thing) locked-for?(x) false;
make-method-overridable(Thing:locked-for?, true);

define method (Thing) moveto(loc) {
  define caller = caller-effuid();
  define oldloc = location(this);

  if (caller != owner(this) && caller != owner(oldloc) && !privileged?(caller))
    return #no-permission;

  if (oldloc != loc) {
    if (!this:pre-move(oldloc, loc) || !loc:pre-accept(this, oldloc))
      return #move-rejected;

    if (valid(oldloc) && isa(oldloc, Thing))
      oldloc:release(this, loc);

    move-to(this, loc);

    loc:post-accept(this, oldloc) && this:post-move(oldloc, loc);
  } else
    true;
}

define method (Thing) pre-move(oldloc, loc) true;
make-method-overridable(Thing:pre-move, true);

define method (Thing) pre-accept(thing, from) false;
make-method-overridable(Thing:pre-accept, true);

define method (Thing) post-accept(thing, from) true;
make-method-overridable(Thing:post-accept, true);

define method (Thing) post-move(oldloc, loc) true;
make-method-overridable(Thing:post-move, true);

define method (Thing) release(what, newloc) undefined;
make-method-overridable(Thing:release, true);

define method (Thing) description() copy-of(this.description);
make-method-overridable(Thing:description, true);

define method (Thing) set-description(d) {
  if (caller-effuid() != owner(this))
    return #no-permission;

  this.description = d;
}

define method (Thing) space() {
  if (caller-effuid() != owner(this) && !privileged?(caller-effuid()))
    return false;

  move-to(this, null);
  return true;
}

////////////////////////////////////////////////////////////////////////////
// VERBS

define method (Thing) setdesc-verb(b) {
  define d = this:description();
  define player = realuid();
  define ptell = player:tell;

  if (effuid() != owner(this))
    ptell("You don't own that object, so you are not allowed to describe it...\n");
  else {
    player:mtell(["Editing description of ", this, ".\n"]);
    define e = editor(d);

    if (e == d)
      ptell("Edit aborted.\n");
    else {
      ptell("Setting description...\n");
      if (this:set-description(e) == #no-permission)
	ptell("Failed to set the description.\n");
      else
	ptell("Description set.\n");
    }
  }
}
set-setuid(Thing:setdesc-verb, false);
Thing:add-verb(#obj, #setdesc-verb, ["@describe ", #obj]);

define method (Thing) @name-verb(b) {
  define player = realuid();
  define name = b[#name][0];
  player:mtell(["Renaming ", this, " to \"", name, "\"...\n"]);
  if (this:set-name(name))
    player:tell("Successful.\n");
  else
    player:tell("Permission denied.\n");
}
set-setuid(Thing:@name-verb, false);
Thing:add-verb(#obj, #@name-verb, ["@name ", #obj, " as ", #name]);

define method (Thing) @alias-verb(b) {
  define player = realuid();
  define name = b[#name][0];
  player:mtell(["Aliasing ", this, " to \"", name, "\"...\n"]);
  if (this:add-alias(name))
    player:tell("Successful.\n");
  else
    player:tell("Permission denied.\n");
}
set-setuid(Thing:@alias-verb, false);
Thing:add-verb(#obj, #@alias-verb, ["@alias ", #obj, " as ", #name]);

define method (Thing) look() {
  [
   "\n",
   this.name, "\n",
   make-string(length(this.name), "~") + "\n"
  ] + map(function (x) pronoun-sub(x, this), this:description());
}
make-method-overridable(Thing:look, true);

define method (Thing) look-verb(bindings) {
  realuid():mtell(this:look());
}
Thing:add-verb(#obj, #look-verb, ["look ", #obj]);
Thing:add-verb(#obj, #look-verb, ["look at ", #obj]);

define method (Thing) examine() {
  define s = this.name + " (owned by " + owner(this).name + ")\n";

  s = s + "Location: " + get-print-string(location(this)) + "\n";

  {
    define al = this:aliases();

    if (length(al) > 0) {
      s = s + "Aliases: " + al[0];
      s = reduce(function (acc, al) acc + ", " + al, s, section(al, 1, length(al) - 1)) + "\n";
    }
  }

  {
    define pa = parents(this);

    if (length(pa) > 0) {
      s = s + "Parent(s): " + pa[0].name;
      s = reduce(function (acc, par) acc + ", " + par.name, s, section(pa, 1, length(pa) - 1)) +
	"\n";
    }
  }

  {
    define l = location(this);

    if (valid(l))
      s = s + "Location: " + l.name + "\n";
  }

  s = s + "Methods: " + string-wrap-string(get-print-string(methods(this)), 0) + "\n";
  s = s + "Slots: " + string-wrap-string(get-print-string(slots(this)), 0) + "\n";
  s = s + "Verbs: " + string-wrap-string(get-print-string(map(function (v) v[2], this.verbs)),
					 0) + "\n";

  s;
}
make-method-overridable(Thing:examine, true);

define method (Thing) @examine-verb(b) {
  realuid():tell(this:examine());
}
Thing:add-verb(#obj, #@examine-verb, ["@examine ", #obj]);

define method (Thing) @who-verb(b) {
  define p = realuid();
  p:tell("Currently connected players:\n");

  lock(Login);
  define ap = Login.active-players;
  unlock(Login);

  for-each(function (pl) p:tell("\t" + pl.name + " is in " + location(pl).name + ".\n"), ap);
}
Thing:add-verb(#this, #@who-verb, ["@who"]);

define method (Thing) @verbs-verb(b) {
  define vbs = ["Verbs defined on " + this.name + " and it's parents:\n"];

  define function tellverbs(x) {
    vbs = vbs + ["  (" + x.name + ")\n"];
    for-each(function (v) {
      vbs = vbs + [
		   "\t" +				// get-print-string(v[2]) + ": " +
		   reduce(function (acc, e) acc + (if (type-of(e) == #symbol)
						   "<" + get-print-string(e) + ">";
						   else e),
			  "",
			  v[0]) +
		   "\n"];
    }, x.verbs);

    if (x != Thing)
      for-each(tellverbs, parents(x));
  }

  tellverbs(this);
  realuid():mtell(vbs);
}
Thing:add-verb(#obj, #@verbs-verb, ["@verbs ", #obj]);

define method (Thing) @space-verb(b) {
  if (this:space())
    realuid():tell("You have @spaced " + get-print-string(this) + ".\n");
  else
    realuid():tell("You have no permission to @space that object.\n");
}
set-setuid(Thing:@space-verb, false);
Thing:add-verb(#obj, #@space-verb, ["@space ", #obj]);

checkpoint();
shutdown();
.
