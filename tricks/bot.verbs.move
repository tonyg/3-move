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
