// player.move -- also stuff on Wizard!
set-realuid(Wizard);
set-effuid(Wizard);

clone-if-undefined(#Player, Thing);
set-object-flags(Player, O_NORMAL_FLAGS);
move-to(Player, null);

Player:set-description(["You see a player who needs to @describe %r.\n"]);

define method (Player) clone() {
  if (!privileged?(effuid()))
    undefined;
  else
    as(Thing):clone();
}
set-setuid(Player:clone, false);

define method (Player) initialize() {
  set-owner(this, this);
  this:set-name("NewPlayer");
  lock(Login);
  if (index-of(Login.players, this) == false)
    Login.players = Login.players + [this];
  unlock(Login);
  as(Thing):initialize();
}
set-method-flags(Player:initialize, O_OWNER_MASK);

define method (Player) new(newname) {
  define p = Player:clone();
  p:set-name(newname);
  p;
}
set-setuid(Player:new, false);
set-method-flags(Player:new, O_OWNER_MASK);

Player:set-name("Generic Player");

define method (Player) set-name(n) {
  if (!privileged?(caller-effuid()))
    false;
  else
    as(Thing):set-name(n);
}

define (Player) password = "";
set-slot-flags(Player, #password, O_OWNER_MASK);

define method (Player) set-password(newpass) {
  if (caller-effuid() != owner(this) && !privileged?(caller-effuid()))
    return false;

  this.password = newpass;
  true;
}

define (Player) connection = null;
set-slot-flags(Player, #connection, O_OWNER_MASK);

define (Player) awake = false;
set-slot-flags(Player, #awake, O_ALL_R);

define (Player) home = null;
set-slot-flags(Player, #home, O_ALL_R);

define (Player) gender = #neuter;
set-slot-flags(Player, #gender, O_ALL_R | O_OWNER_MASK);

define (Player) screen-length = 24;
set-slot-flags(Player, #screen-length, O_ALL_R | O_OWNER_MASK);

define (Player) screen-width = 80;
set-slot-flags(Player, #screen-width, O_ALL_R | O_OWNER_MASK);

define method (Player) set-home(newhome) {
  if (caller-effuid() != owner(this) && !privileged?(caller-effuid()))
    return false;

  this.home = newhome;
  true;
}

define (Player) is-programmer = false;
set-slot-flags(Player, #is-programmer, O_ALL_R);

define method (Player) set-programmer(v) {
  if (caller-effuid() != owner(this) && !privileged?(caller-effuid()))
    return false;

  this.is-programmer = v;
  true;
}

define method (Player) connect-function() {
  if (location(this) == this.home)
    this:mtell(location(this):look() + ["\n"]);
  else
    this:moveto(this.home);
  location(this):announce(this.name + " wakes up.\n");
}
set-setuid(Player:connect-function, false);
make-method-overridable(Player:connect-function, true);

define method (Player) disconnect-function() {
  location(this):announce(this.name + " goes to sleep.\n");
  this:moveto(this.home);
}
set-setuid(Player:disconnect-function, false);
make-method-overridable(Player:disconnect-function, true);

define method (Player) match-object(name) {
  if (equal?(name, "me"))
    return this;

  return as(Thing):match-object(name);
}
make-method-overridable(Player:match-object, true);

define method (Player) read() {
  if (type-of(this.connection) == #connection)
    read-from(this.connection);
  else
    "";
}

define method (Player) mtell(v) {
  if (type-of(v) == #vector) {
    if (this:listening?()) {
      if (realuid() == this) {
	define i = 0;
	define count = 0;

	while (i < length(v)) {
          this:tell(v[i]);
	  i = i + 1;
	  count = count + 1;

	  if (this.screen-length && count >= this.screen-length) {
            this:tell("Press ENTER for more, or type q to quit:");
	    if (equal?(this:read(), "q"))
	      return;
	    count = 0;
	  }
	}
      } else
	for-each(this:tell, v);
    }
  } else
    this:tell(v);
}

define method (Player) tell(x) {
  if (this:listening?())
    print-on(this.connection, get-print-string(x));
}

define method (Player) listening?()
  this.awake && type-of(this.connection) == #connection;

define method (Player) pre-accept(what, oldloc)
  !isa(what, Player);
make-method-overridable(Player:pre-accept, true);

define method (Player) release(what, newloc)
  realuid() == this;
make-method-overridable(Player:release, true);

////////////////////////////////////////////////////////////////
// WIZARDY STUFF

set-parents(Wizard, Player);
Wizard:initialize();
Wizard.name = "Wizard";
Wizard.is-programmer = true;

define method (Wizard) @shutdown-verb(b) {
  fork/quota(function () {
    sleep(1);
    checkpoint();
    shutdown();
  }, VM_STATE_NOQUOTA);

  close(this.connection);
}
set-setuid(Wizard:@shutdown-verb, false);
set-method-flags(Wizard:@shutdown-verb, O_OWNER_MASK);
Wizard:add-verb(#this, #@shutdown-verb, ["@shutdown"]);

define method (Wizard) @checkpoint-verb(b) {
  checkpoint();
  this:tell("Checkpoint done.\n");
}
set-setuid(Wizard:@checkpoint-verb, false);
set-method-flags(Wizard:@checkpoint-verb, O_OWNER_MASK);
Wizard:add-verb(#this, #@checkpoint-verb, ["@checkpoint"]);

////////////////////////////////////////////////////////////////
// VERBS ON Player

define method (Player) look() {
  as(Thing):look() + [ "(" + this.name + " is " +
		     (if (this.awake)
		       "awake";
		     else
		       "asleep") + ".)\n" ];
}
make-method-overridable(Player:look, true);

define method (Player) room-look-verb(b) {
  realuid():mtell(location(this):look());
}
Player:add-verb(#this, #room-look-verb, ["look"]);

define method (Player) @setpass-verb(b) {
  define pass = b[#pass][0];

  realuid():tell(if (this:set-password(pass))
		   "Password changed.\n";
		 else
		   "Password not changed (permission denied).\n");
}
set-setuid(Player:@setpass-verb, false);
Player:add-verb(#this, #@setpass-verb, ["@setpass ", #pass]);

define method (Player) @gender-verb(b) {
  if (caller-effuid() != owner(this))
    return;

  define g = b[#gender][0];
  define gs = as-sym(g);

  if (Root.Genders[gs] == undefined)
    realuid():tell("That gender is not defined, sorry.\n");
  else {
    this.gender = gs;
    realuid():mtell(["Your gender is now set to ", gs, ".\n"]);
  }
}
Player:add-verb(#this, #@gender-verb, ["@gender ", #gender]);

define method (Player) get-verb(b) {
  define oname = b[#obj][0];
  define o = b[#obj][1];

  define function take1(x) {
    define oldloc = location(x);

    if (oldloc == this) {
      this:mtell(["You were already carrying ", x.name, ".\n"]);
    } else if (x:moveto(this) == true) {
      this:mtell(["You pick up ", x.name, ".\n"]);
      oldloc:announce([this.name, " picks up ", x.name, ".\n"]);
    } else {
      this:mtell(["You fail to pick up ", x.name, ".\n"]);
      oldloc:announce([this.name, " tries to pick up ", x.name, ", but fails.\n"]);
    }
  }

  if (o) {
    take1(o);
    return;
  }

  if (equal?(oname, "all") || equal?(oname, "everything")) {
    this:tell("You begin picking up everything you can see.\n");
    for-each(take1, contents(location(this)));
  } else
    this:tell("There doesn't appear to be anything called \"" + b[#obj][0] + "\" here.\n");
}
set-setuid(Player:get-verb, false);
make-method-overridable(Player:get-verb, true);
for-each(function (pattern) Player:add-verb(#this, #get-verb, pattern),
	 [
	  ["get ", #obj],
	  ["pick up ", #obj],
	  ["take ", #obj]
	 ]);

define method (Player) drop-verb(b) {
  define oname = b[#obj][0];
  define o = b[#obj][1];

  define function drop1(x) {
    if (!index-of(contents(this), x))
      this:mtell(["You aren't holding ", x.name, ".\n"]);
    else if (x:moveto(location(this)) == true) {
      this:mtell(["You drop ", x.name, ".\n"]);
      location(this):announce([this.name, " drops ", x.name, ".\n"]);
    } else
      this:mtell(["You find that you cannot drop ", x.name, " here.\n"]);
  }

  if (o) {
    drop1(o);
    return;
  }

  if (equal?(oname, "all") || equal?(oname, "everything")) {
    this:tell("You begin dropping everything you are carrying.\n");
    for-each(drop1, contents(this));
  } else
    this:tell("You can't find anything called \"" + oname + "\" to drop.\n");
}
set-setuid(Player:drop-verb, false);
make-method-overridable(Player:drop-verb, true);
for-each(function (pattern) Player:add-verb(#this, #drop-verb, pattern),
	 [
	  ["drop ", #obj],
	  ["put down ", #obj]
	 ]);

define method (Player) invent-verb(b) {
  define s = ["You are holding:\n"];

  if (length(contents(this)) == 0)
    s = s + ["    nothing.\n"];
  else
    for-each(function (x) s = s + ["    " + x.name + "\n"], contents(this));

  this:mtell(s);
}
make-method-overridable(Player:invent-verb, true);
Player:add-verb(#this, #invent-verb, ["invent"]);
Player:add-verb(#this, #invent-verb, ["inventory"]);

define method (Player) @setlines-verb(b) {
  if (caller-effuid() != owner(this))
    return;

  define l = as-num(b[#numlines][0]);

  if (l) {
    this.screen-length = l;
    realuid():tell("Your screen length is set to " + get-print-string(l) + ".\n");
  } else {
    this.screen-length = false;
    realuid():tell("Page-at-a-time-mode has been switched off.\n");
  }
}
make-method-overridable(Player:@setlines-verb, true);
Player:add-verb(#this, #@setlines-verb, ["@setlines ", #numlines]);

define method (Player) @setcolumns-verb(b) {
  if (caller-effuid() != owner(this))
    return;

  define l = as-num(b[#numcols][0]);

  if (l) {
    this.screen-width = l;
    realuid():tell("Your screen width is set to " + get-print-string(l) + ".\n");
  } else
    realuid():tell("You have entered an invalid screen width. Setting not changed.\n");
}
make-method-overridable(Player:@setcolumns-verb, true);
Player:add-verb(#this, #@setcolumns-verb, ["@setcolumns ", #numcols]);

define method (Player) @sethome-verb(b) {
  if (caller-effuid() != owner(this))
    return;

  define loc = b[#loc][1];
  define player = realuid();

  if (loc) {
    if (!isa(loc, Room)) {
      player:tell("That place isn't a room!\n");
      return;
    }

    define function do-the-move() {
      set-effuid(this);
      this:moveto(loc);
    }

    if (do-the-move() == true) {
      player:tell("You set your home to " + loc.name + ".\n");
      this.home = loc;
      return;
    }
  }

  player:tell("You can't seem to set your home to that particular object.\n");
}
make-method-overridable(Player:@sethome-verb, true);
Player:add-verb(#this, #@sethome-verb, ["@sethome ", #loc]);

define method (Player) home-verb(b) {
  if (caller-effuid() != owner(this))
    return;

  location(this):announce(this.name + " decides to go home.\n");
  this:moveto(this.home);
  location(this):announce(this.name + " arrives home.\n");
}
Player:add-verb(#this, #home-verb, ["home"]);

define method (Player) @space-verb(b) {
  realuid():tell("You can't @space players.\n");
}

define method (Player) @quit-verb(b) {
  if (caller-effuid() != owner(this))
    return;

  close(this.connection);
}
Player:add-verb(#this, #@quit-verb, ["@quit"]);

define method (Player) @build-verb(b) {
  define pname = b[#parent-obj][0];
  define pval = b[#parent-obj][1];
  define oname = b[#new-name][0];
  define ptell = realuid():tell;

  if (!pval) {
    pval = symbol-value(as-sym(pname));
    if (type-of(pval) != #object) {
      ptell("I don't recognise that name for a parent object, sorry.\n");
      return false;
    }
  }

  if (object-named(oname)) {
    ptell("That name already exists, searching for a replacement...\n");
    define i = 1;
    while (object-named(oname + get-print-string(i))) i = i + 1;
    oname = oname + get-print-string(i);
    ptell("Replacement name: " + oname + "\n");
  }

  define obj = pval:clone();

  if (type-of(obj) == #object) {
    ptell("You created an object.\n");
    if (obj:set-name(oname))
      ptell("You named it \"" + oname + "\".\n");
    else
      ptell("You couldn't name it, for some reason.\n");
  } else
    ptell("You didn't create an object.\n");
}
set-setuid(Player:@build-verb, false);
Player:add-verb(#this, #@build-verb, ["@build ", #parent-obj, " as ", #new-name]);

checkpoint();
shutdown();
.
