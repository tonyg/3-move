// thing.move
set-realuid(Wizard);
set-effuid(Wizard);

define Thing = Root:clone();
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
set-method-flags(Thing:initialize, O_ALL_PERMS | O_C_FLAG);

define method (Thing) add-alias(n) {
  if (caller-effuid() != owner(this) && !privileged?(caller-effuid()))
    return false;

  if (slot-clear?(this, #aliases))
    this.aliases = [n];
  else
    this.aliases = this.aliases + [n];
  return true;
}
set-method-flags(Thing:add-alias, O_ALL_PERMS);

define method (Thing) aliases() {
  (if (slot-clear?(this, #aliases)) []; else this.aliases) +
  (if (this == Thing) []; else
    reduce(function (acc, par) acc + par:aliases(), this.aliases, parents(this)));
}
set-method-flags(Thing:aliases, O_ALL_PERMS);

define method (Thing) add-verb(selfvar, methname, pattern) {
  define c = caller-effuid();
  define fl = object-flags(this);

  if ((fl & O_WORLD_W == O_WORLD_W) ||
      ((fl & O_GROUP_W == O_GROUP_W) && in-group-of(c, this)) ||
      ((fl & O_OWNER_W == O_OWNER_W) && c == owner(this)) ||
      privileged?(c)) {
    this.verbs = this.verbs + [[pattern, selfvar, methname]];
    true;
  } else
    false;
}
set-method-flags(Thing:add-verb, O_ALL_PERMS);

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
	if (idx + 1 == patlen && length(rsent) > 0) {
	  bindings[thiselt] = rsent;
	  return true;
	}

	define nextelt = pat[idx + 1];

	if (type-of(nextelt) != #string)
	  return false;

	define nel = length(nextelt);
	define loc = substring-search(rsent, nextelt);

	if (!loc)
	  return false;

	bindings[thiselt] = section(rsent, 0, loc);
	return accumulate-matches(section(rsent, loc + nel, length(rsent) - (loc + nel)), idx + 2);
      } else if (type-of(thiselt) == #string) {
	if (substring-search(rsent, thiselt) != 0)
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

  (this != Thing && search-vec(parents(this))) || search-vec(contents(this));
}
set-method-flags(Thing:match-verb, O_ALL_PERMS);

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
set-method-flags(Thing:match-objects, O_ALL_PERMS);

define method (Thing) match-object(name) {
  if (equal?(name, this.name))
    return this;

  define c = first-that(function (x) equal?(x, name), this:aliases());
  if (c != undefined)
    return this;

  define c = find-by-name(contents(this), name);
  if (c != undefined)
    return c;

  c = first-that(function (x) x:match-object(name), contents(this));
  if (c != undefined)
    return c;

  return false;
}
set-method-flags(Thing:match-object, O_ALL_PERMS | O_C_FLAG);

define method (Thing) read() "";
set-method-flags(Thing:read, O_ALL_PERMS | O_C_FLAG);

define method (Thing) mtell(vec) for-each(this:tell, vec);
set-method-flags(Thing:mtell, O_ALL_PERMS);

define method (Thing) tell(x) undefined;
set-method-flags(Thing:tell, O_ALL_PERMS | O_C_FLAG);

define method (Thing) listening?() false;
set-method-flags(Thing:listening?, O_ALL_PERMS | O_C_FLAG);

define method (Thing) moveto(loc) {
  define caller = caller-effuid();

  if (caller != owner(this))
    return #no-permission;

  define oldloc = location(this);

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
set-method-flags(Thing:moveto, O_ALL_PERMS);

define method (Thing) pre-move(oldloc, loc) true;
set-method-flags(Thing:pre-move, O_ALL_PERMS | O_C_FLAG);

define method (Thing) pre-accept(thing, from) false;
set-method-flags(Thing:pre-accept, O_ALL_PERMS | O_C_FLAG);

define method (Thing) post-accept(thing, from) true;
set-method-flags(Thing:post-accept, O_ALL_PERMS | O_C_FLAG);

define method (Thing) post-move(oldloc, loc) true;
set-method-flags(Thing:post-move, O_ALL_PERMS | O_C_FLAG);

define method (Thing) description() copy-of(this.description);
set-method-flags(Thing:description, O_ALL_PERMS);

define method (Thing) set-description(d) {
  if (caller-effuid() != owner(this))
    return #no-permission;

  this.description = d;
}
set-method-flags(Thing:set-description, O_ALL_PERMS);

////////////////////////////////////////////////////////////////////////////
// VERBS

define method (Thing) setdesc-verb(b) {
  define d = this:description();
  define player = realuid();
  player:mtell(["Editing description of ", this, ".\n"]);
  define e = editor(d);

  if (e == d)
    player:tell("Edit aborted.\n");
  else {
    player:tell("Setting description...\n");
    if (this:set-description(e) == #no-permission)
      player:tell("Permission denied.\n");
    else
      player:tell("Successful.\n");
  }
}
set-setuid(Thing:setdesc-verb, false);
set-method-flags(Thing:setdesc-verb, O_ALL_PERMS);
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
set-method-flags(Thing:@name-verb, O_ALL_PERMS);
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
set-method-flags(Thing:@alias-verb, O_ALL_PERMS);
Thing:add-verb(#obj, #@alias-verb, ["@alias ", #obj, " as ", #name]);

define method (Thing) look() {
  [
   "\n",
   this.name, "\n",
   make-string(length(this.name), "~") + "\n"
  ] + map(function (x) pronoun-sub(x, this), this:description());
}

define method (Thing) look-verb(bindings) {
  realuid():mtell(this:look());
}
Thing:add-verb(#obj, #look-verb, ["look ", #obj]);

define method (Thing) examine() {
  define s = this.name + " (owned by " + owner(this).name + ")\n";

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

  s = s + "Methods: " + get-print-string(methods(this)) + "\n";
  s = s + "Slots: " + get-print-string(slots(this)) + "\n";
  s = s + "Verbs: " + get-print-string(map(function (v) v[2], this.verbs)) + "\n";

  s;
}
set-method-flags(Thing:examine, O_ALL_PERMS | O_C_FLAG);

define method (Thing) @examine-verb(b) {
  realuid():tell(this:examine());
}
set-method-flags(Thing:examine, O_ALL_PERMS);
Thing:add-verb(#obj, #@examine-verb, ["@examine ", #obj]);

define method (Thing) @who-verb(b) {
  define p = realuid();
  p:tell("Currently connected players:\n");

  for-each(function (pl) p:tell("\t" + pl.name + " is in " + location(pl).name + ".\n"),
	   Login.active-players);
}
set-method-flags(Thing:@who-verb, O_ALL_PERMS);
Thing:add-verb(#this, #@who-verb, ["@who"]);

define method (Thing) @verbs-verb(b) {
  define p = realuid();

  p:tell("Verbs defined on " + this.name + " and it's parents:\n");

  define function tellverbs(x) {
    for-each(function (v) {
      p:tell("\t" + get-print-string(v[2]) + ": " +
	     reduce(function (acc, e) acc + (if (type-of(e) == #symbol)
					     "<" + get-print-string(e) + ">";
					     else e), "", v[0]) + "\n");
    }, x.verbs);

    if (x != Thing)
      for-each(tellverbs, parents(x));
  }

  tellverbs(this);
}
set-method-flags(Thing:@verbs-verb, O_ALL_PERMS);
Thing:add-verb(#obj, #@verbs-verb, ["@verbs ", #obj]);

checkpoint();
shutdown();
.
