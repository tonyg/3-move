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
// Routines for periodic checkpointing
// Need to be loaded into the database at server startup - just like db sources
// Run with "move db checkpoint.move" and it will update, save, and quit.

set-realuid(Wizard);
set-effuid(Wizard);

define checkpoint-thread = -1;
define checkpoint-interval = 600;	// 10 minutes

define function start-checkpointing() {
  stop-checkpointing();
  checkpoint-thread = fork/quota(function () {
    while (true) {
      checkpoint();
      sleep(checkpoint-interval);
    }
  }, VM_STATE_DAEMON);
}

define function stop-checkpointing() {
  if (checkpoint-thread > 0) {
    force-kill(checkpoint-thread);
    checkpoint-thread = -1;
  }
}

define function next-checkpoint-time() {
  if (checkpoint-thread > 0) {
    define tab = get-thread-table();
    define i = 0;

    while (i < length(tab)) {
      if ((tab[i][0] == checkpoint-thread) && tab[i][2])
	return tab[i][3];

      i = i + 1;
    }
  }

  return -1;
}

checkpoint();
shutdown();
.
