// room.move
set-realuid(Wizard);
set-effuid(Wizard);

clone-if-undefined(#Room, Thing);
set-object-flags(Room, O_NORMAL_FLAGS | O_C_FLAG);
Room:set-name("Generic Room");
move-to(Room, null);

define method (Room) initialize() {
  as(Thing):initialize();
  this.exits = [];
}
make-method-overridable(Thing:initialize, true);

Player.home = Room;

Room:set-description([
		      "This is a nondescript room.\n"
]);

// 		      "This room is the model for all other rooms in the universe.\n",
// 		      "It is bare, painted pale green, cubical, and is illuminated\n",
// 		      "by a single unshaded electric bulb hanging from a short length\n",
// 		      "of cable from the centre of the ceiling.\n"

define (Room) dark = false;
set-slot-flags(Room, #dark, O_ALL_R | O_OWNER_MASK);

define (Room) exits = [];
set-slot-flags(Room, #exits, O_ALL_R | O_OWNER_MASK);

define (Room) attachable = false;
set-slot-flags(Room, #attachable, O_OWNER_MASK);

define method (Room) set-dark(val) {
  if (caller-effuid() != owner(this) && !privileged?(caller-effuid()))
    return false;

  this.dark = val;
  true;
}
make-method-overridable(Room:set-dark, true);

define method (Room) exits() {
  lock(this);
  define ex = copy-of(this.exits);
  unlock(this);
  ex;
}

define method (Room) can-modify-exits?(who) {
  return (who == owner(this) || privileged?(who) || this.attachable);
}
make-method-overridable(Room:can-modify-exits?, true);

define method (Room) attachable?() this.attachable;
define method (Room) set-attachable(val) {
  if (caller-effuid() != owner(this) && !privileged?(caller-effuid()))
    return false;

  this.attachable = val;
  true;
}

define method (Room) add-exit(ex) {
  if (!this:can-modify-exits?(caller-effuid()) || !isa(ex, Exit))
    return false;

  lock(this);
  this.exits = this.exits + [ex];
  unlock(this);
  true;
}

define method (Room) del-exit(ex) {
  define caller = caller-effuid();

  if (!this:can-modify-exits?(caller) || (caller != owner(ex) && !privileged?(caller)))
    return false;

  lock(this);
  define idx = index-of(this.exits, ex);
  if (idx)
    this.exits = delete(this.exits, idx, 1);
  unlock(this);
  return idx && true;	// just to make it return either "true" _or_ "false".
}

define method (Room) pre-accept(x, oldloc) true;
make-method-overridable(Room:pre-accept, true);

define method (Room) post-accept(x, oldloc) {
  if (x:listening?())
    x:mtell(this:look());
  true;
}
make-method-overridable(Room:post-accept, true);

define method (Room) match-verb(sent) {
  define result = as(Thing):match-verb(sent);

  if (result)
    return result;

  first-that(function (ex) result = ex:match-verb(sent), this:exits());
  result;
}

define method (Room) match-object(name) {
  if (equal?(name, "here"))
    return this;

  define c = first-that(function (ex) ex:match-object(name), this:exits());
  if (c != undefined)
    return c;

  return as(Thing):match-object(name);
}
make-method-overridable(Room:match-object, true);

define method (Room) huh?(sent) {
  if (length(sent) >= 5 && equal?(section(sent, 0, 5), "look ")) {
    realuid():tell("You can't see anything called \"" + section(sent, 5, length(sent) - 5) +
		   "\" here.\n");
    return;
  }

  define exits = this:exits();
  define i = 0;

  while (i < length(exits)) {
    define ex = exits[i];

    if (ex:matches-name(sent)) {
      ex:transport(realuid());
      return;
    }

    i = i + 1;
  }

  realuid():tell("I don't understand that (try \"@verbs me\", \"@verbs here\" etc).\n");
}
set-setuid(Room:huh?, false);
make-method-overridable(Room:huh?, true);

Player.home = Room;

////////////////////////////////////////////////////////////////
// VERBS

define method (Room) look() {
  // Redefines Thing:look.
  define val = [];

  if (!this.dark) {
    define objects = [];
    define players = [];

    for-each(function (o) {
      if (isa(o, Player))
	players = players + [o];
      else
	objects = objects + [o];
    }, contents(this));

    if (length(objects) > 0)
      val = val + wrap-string("You see " + make-sentence(objects) + " here.\n", 0);

    if (length(players) > 0) {
      if (length(players) == 1)
	val = val + [ players[0].name + " is here.\n" ];
      else
	val = val + wrap-string(make-sentence(players) + " are here.\n", 0);
    }
  }

  define exits = this:exits();
  if (length(exits) > 0)
    val = val + wrap-string(
			    "Obvious exits: " +
			    reduce(function (acc, ex) acc + ", " + ex.name + " to " + ex.dest.name,
				   exits[0].name + " to " + exits[0].dest.name,
				   section(exits, 1, length(exits) - 1)) +
			    "\n",
			    0);

  as(Thing):look() + val;
}
make-method-overridable(Room:look, true);

define method (Room) say-verb(b) {
  define sent = b[#sent][0];

  this:announce(realuid().name + " says, \"" + sent + "\"\n");
  realuid():tell("You say, \"" + sent + "\"\n");
}
make-method-overridable(Room:say-verb, true);
Room:add-verb(#this, #say-verb, ["say ", #sent]);

define method (Room) emote-verb(b) {
  this:announce-all(realuid().name + b[#sent][0] + "\n");
}
make-method-overridable(Room:emote-verb, true);
Room:add-verb(#this, #emote-verb, ["emote", #sent]);

define method (Room) @@shout-verb(b) {
  lock(Login);
  define ap = Login.active-players;
  unlock(Login);

  define player = realuid();
  define message = [
		    player.name + " shouts from " + location(player):fullname() + ":\n",
		    "\t" + b[#sent][0] + "\n"
		   ];

  for-each(function (p) {
    p:mtell(message);
  }, ap);
}
Room:add-verb(#this, #@@shout-verb, ["@@shout ", #sent]);

define method (Room) enter-verb(b) {
  define p = realuid();

  location(p):announce(p.name + " vanishes suddenly.\n");
  p:moveto(this);
  location(p):announce(p.name + " appears suddenly.\n");
}
set-setuid(Room:enter-verb, false);
Room:add-verb(#room, #enter-verb, ["enter ", #room]);
Room:add-verb(#room, #enter-verb, ["go to ", #room]);

define method (Room) @dig-verb(b) {
  define name = b[#name][0];
  define backlinkname = b[#backlinkname];
  define destname = b[#dest][0];
  define dest = b[#dest][1];
  define ptell = realuid():tell;
  define created-dest = false;

  if (backlinkname == undefined)
    backlinkname = "out";
  else
    backlinkname = backlinkname[0];

  if (dest) {
    if (!isa(dest, Room)) {
      ptell("The destination object either isn't a room or can't be found by that name.\n");
      return false;
    }
    ptell("Note that the destination exists, so no backlink will be created.\n");
  } else {
    created-dest = true;
    dest = Room:clone();
    dest:set-name(destname);
    if (Exit:dig(backlinkname, dest, this))
      ptell("You dig the backlink exit, named \"" +
	    backlinkname + "\", from \"" +
	    dest:fullname() + "\" to \"" +
	    this:fullname() + "\".\n");
    else
      ptell("You fail to dig the backlink.\n");
  }

  if (!this:can-modify-exits?(effuid())) {
    ptell("You don't have permission to @dig here.\n");
    return false;
  }

  if (Exit:dig(name, this, dest)) {
      ptell("You dig the outward exit, named \"" + name + "\", from \"" + this:fullname() +
	    "\" to \"" + dest:fullname() + "\".\n");
      if (created-dest)
	dest:space();
  } else
      ptell("You fail to dig the outward link.\n");
}
set-setuid(Room:@dig-verb, false);
Room:add-verb(#loc, #@dig-verb, ["@dig ", #loc, " to ", #dest, " as ", #name]);
Room:add-verb(#loc, #@dig-verb, ["@dig2 ", #loc, " to ", #dest,
				" as ", #name, " and ", #backlinkname]);

define method (Room) @undig-verb(b) {
  define name = b[#name][0];
  define ptell = realuid():tell;

  if (!this:can-modify-exits?(effuid())) {
    ptell("You don't have permission to @undig here.\n");
    return false;
  }

  if (this:del-exit(find-by-name(this:exits(), name)))
    ptell("You remove the exit named " + name + ".\n");
  else
    ptell("You fail to @undig the exit " + name + ".\n");
}
set-setuid(Room:@undig-verb, false);
Room:add-verb(#loc, #@undig-verb, ["@undig ", #name, " from ", #loc]);

define method (Room) @setattach-verb(b) {
  define val = b[#yes/no/true/false][0];
  define ptell = realuid():tell;

  val = !(equal?(val, "no") || equal?(val, "false"));
  if (this:set-attachable(val))
    ptell("You succeed in @setattaching " + this.name + ".\n");
  else
    ptell("You fail to @setattach " + this.name + ".\n");
}
set-setuid(Room:@setattach-verb, false);
Room:add-verb(#loc, #@setattach-verb, ["@setattach ", #loc, " to ", #yes/no/true/false]);

checkpoint();
shutdown();
.
