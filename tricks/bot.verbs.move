// TARGET this program to Generic Wanderbot.

define (TARGET) walker-thread = false;
set-slot-flags(TARGET, #walker-thread, O_OWNER_MASK);

define method (TARGET) look() {
  if (this.walker-thread)
    as(Thing):look() + [ pronoun-sub("%S is awake and alert.\n", this) ];
  else
    as(Thing):look() + [ pronoun-sub("%S is not moving.\n", this) ];
}
make-method-overridable(TARGET:look, true);

define method (TARGET) start-walking() {
  if (!this.walker-thread) {
    define walker-fn = function () {
      define loc = location(this);

      if (isa(loc, Room)) {
	define ex = loc:exits();
	if (length(ex) > 0)
	  ex[random(length(ex))]:transport(this);
      } else
	this:moveto(location(loc));

      sleep(random(15));
      this.walker-thread = fork(walker-fn);
    }

    this.walker-thread = fork(walker-fn);
  }

  true;
}

define method (TARGET) stop-walking() {
  if (this.walker-thread) {
    define res = kill(this.walker-thread, #die, null);
    this.walker-thread = false;
    return res;
  } else
    true;
}

define method (TARGET) start-verb(b) {
  define player = realuid();
  if (this:start-walking()) {
    location(player):announce([player.name + " switches the robot on.\n"]);
    player:tell("You switch the robot on.\n");
  } else {
    location(player):announce([player.name + " tries unsuccessfully to switch the robot on.\n"]);
    player:tell("You fail to start the robot.\n");
  }
}
TARGET:add-verb(#bot, #start-verb, ["start ", #bot]);

define method (TARGET) stop-verb(b) {
  define player = realuid();
  if (this:stop-walking()) {
    location(player):announce([player.name + " switches the robot off.\n"]);
    player:tell("You switch the robot off.\n");
  } else {
    location(player):announce([player.name + " tries unsuccessfully to switch the robot off.\n"]);
    player:tell("You fail to stop the robot.\n");
  }
}
TARGET:add-verb(#bot, #stop-verb, ["stop ", #bot]);
