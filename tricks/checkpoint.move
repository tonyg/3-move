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
