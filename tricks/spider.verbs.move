// Spider that registers all accessible rooms and their contents.
// Target this at your spider object.

define (TARGET) walker-thread = false;
set-slot-flags(TARGET, #walker-thread, O_OWNER_MASK);

define method (TARGET) look() {
  define status = (if (this.walker-thread) "awake and alert"; else "not moving");
  as(Thing):look() + [ pronoun-sub("%S is " + status + ".\n", this) ];
}
make-method-overridable(TARGET:look, true);

define method (TARGET) crawler-body(obj, numsteps) {
  realuid():tell("Found " + obj:fullname() + " " + get-print-string(numsteps) + " steps away.\n");
}
make-method-overridable(TARGET:crawler-body, true);

define method (TARGET) process(root-object) {
  define orgloc = location(this);

  define function already-met?(x, stk) {
    if (stk == null)
      return false;
    if (x == stk[0])
      return true;
    return already-met?(x, stk[1]);
  }

  define function crawl(o, last, stk, n) {
    o:tell(this.name + " inspects you and the things you are carrying.\n");
    this:crawler-body(o, n);

    stk = [o, stk];

    for-each(function (x) if (!already-met?(x, stk)) crawl(x, o, stk, n + 1), contents(o));

    if (isa(o, Room)) {
      define locnow = location(this);
      define backexit = false;

      for-each(function (x) {
	if (x.dest != last) {
          x:transport(this);
	  if (already-met?(x.dest, stk))
	    x.dest:announce-all(this.name + " looks disoriented for a second, then turns around " +
				"and leaves again.\n");
	  else
	    crawl(x.dest, o, stk, n + 1);

	  if (location(this) != locnow) {
	    x.dest:announce-all(this.name + " crawls into a crack in the ground.\n");
	    this:moveto(locnow);
	  }
	} else
	  backexit = x;
      }, o.exits);

      if (backexit)
	backexit:transport(this);
    }

    o:tell(this.name + " seems satisfied, and moves away.\n");
  }

  crawl(root-object, null, null, 0);
}

define method (TARGET) start-walking() {
  if (!this.walker-thread) {
    define uid = realuid();

    define function walker() {
      set-realuid(uid);
      define o = location(this);
      o:announce-all(this.name + " suddenly starts moving.\n");
      this:process(o);
      o:announce-all(pronoun-sub("%N returns from %p travels.\n" + 
				 "%S comes to a halt, and goes to sleep.\n",
				 this));
      this.walker-thread = false;
    }

    this.walker-thread = fork/quota(walker, -2);
  }

  true;
}

define method (TARGET) stop-walking() {
  if (this.walker-thread) {
    if (get-thread-status(this.walker-thread)) {
      define res = kill(this.walker-thread, #die, null);
      if (res)
	this.walker-thread = false;
      return res;
    } else
      this.walker-thread = false;
  }

  true;
}

define method (TARGET) start-verb(b) {
  define player = realuid();
  if (this:start-walking()) {
    location(player):announce([player.name + " has switched the robot on.\n"]);
    player:tell("You have switched the robot on.\n");
  } else {
    location(player):announce([player.name + " tries unsuccessfully to switch the robot on.\n"]);
    player:tell("You fail to start the robot.\n");
  }
}
TARGET:add-verb(#spider, #start-verb, ["start ", #spider]);

define method (TARGET) stop-verb(b) {
  define player = realuid();
  if (this:stop-walking()) {
    location(player):announce([player.name + " has switched the robot off.\n"]);
    player:tell("You have switched the robot off.\n");
  } else {
    location(player):announce([player.name + " tries unsuccessfully to switch the robot off.\n"]);
    player:tell("You fail to stop the robot.\n");
  }
}
TARGET:add-verb(#spider, #stop-verb, ["stop ", #spider]);
