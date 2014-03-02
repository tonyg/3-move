/*
 * 3-MOVE, a multi-user networked online text-based programmable virtual environment
 * Copyright 1997, 1998, 1999, 2003, 2005, 2008, 2009 Tony Garnock-Jones <tonyg@kcbbs.gen.nz>
 *
 * 3-MOVE is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * 3-MOVE is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with 3-MOVE.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "global.h"
#include "object.h"
#include "vm.h"
#include "prim.h"
#include "gc.h"
#include "primload.h"
#include "scanner.h"
#include "parser.h"
#include "conn.h"
#include "thread.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <unistd.h>
#include <fcntl.h>

typedef struct repl_data {
  VMREGS vmregs;
  OBJ h1, h2;
} repl_data, *REPL_DATA;

PRIVATE void compile_main(FILE *conni, FILE *conno) {
  REPL_DATA rd = allocmem(sizeof(repl_data));
  VMstate vms;

  rd->h1 = rd->h2 = NULL;

  protect(&rd->h1);
  protect(&rd->h2);

  rd->vmregs = (VMREGS) newvector(NUM_VMREGS);	/* dodgy casting :-) */
  vms.r = rd->vmregs;
  protect((OBJ *)(&rd->vmregs));

  init_vm(&vms);
  vms.c.vm_state = VM_STATE_NOQUOTA;

  while (vms.c.vm_state != VM_STATE_DYING) {
    ScanInst si;
    char buf[16384];

    rd->h1 = (OBJ) newbvector(0);

    while (1) {
      char *result;

      result = fgets(buf, 256, conni);

      if (result == NULL)
	break;

      while (1) {
	int l = strlen(buf);
	if (buf[l-1] == '\r' || buf[l-1] == '\n')
	  buf[l-1] = '\0';
	else
	  break;
      }
      strcat(buf, "\n");

      if (!strcmp(buf, ".\n"))
	break;

      rd->h2 = (OBJ) newstring(buf);
      rd->h1 = (OBJ) bvector_concat((BVECTOR) rd->h1, (BVECTOR) rd->h2);
    }

    gc_reach_safepoint();

    rd->h2 = (OBJ) newstringconn((BVECTOR) rd->h1);
    fill_scaninst(&si, (OVECTOR) rd->h2);

    while (!conn_closed((OVECTOR) rd->h2)) {
      rd->h1 = (OBJ) parse(&vms, &si);
      gc_reach_safepoint();

      if (rd->h1 == NULL) {
	sprintf(buf, "-->! the compiler returned NULL.\n");
      } else {
	vms.c.vm_state = VM_STATE_NOQUOTA;

	ATPUT((OVECTOR) rd->h1, ME_OWNER, (OBJ) vms.r->vm_uid);
	vms.r->vm_effuid = vms.r->vm_uid;
	{
	  OVECTOR c = newovector_noinit(CL_MAXSLOTINDEX, T_CLOSURE);
	  ATPUT(c, CL_SELF, NULL);
	  ATPUT(c, CL_METHOD, rd->h1);
	  rd->h1 = (OBJ) c;
	}
	apply_closure(&vms, (OVECTOR) rd->h1, newvector_noinit(1));

	while (!run_vm(&vms)) ;

	rd->h1 = (OBJ) newvector(2);
	ATPUT((VECTOR) rd->h1, 0, NULL);
	ATPUT((VECTOR) rd->h1, 1, vms.r->vm_acc);
	rd->h1 = lookup_prim(0x00001, NULL)(&vms, (VECTOR) rd->h1);
	rd->h1 = (OBJ) bvector_concat((BVECTOR) rd->h1, newbvector(1));
      	/* terminates C-string */

	gc_reach_safepoint();

	sprintf(buf, "--> %s\n", ((BVECTOR) rd->h1)->vec);
      }

      fputs(buf, conno);
    }
  }

  unprotect((OBJ *)(&rd->vmregs));
  unprotect(&rd->h2);
  unprotect(&rd->h1);

  freemem(rd);
}

PRIVATE void run_finalize_queue(void) {
  OBJ *x;

  x = allocmem(sizeof(OBJ));
  *x = NULL;

  protect(x);

  wait_for_finalize();

  *x = next_to_finalize();

  while (*x != NULL) {
    if (OVECTORP(*x)) {
      finalize_ovector((OVECTOR) (*x));
    } else {
      /* %%% NOTE here's where objects should be handed to user level
	 to finalize. %%% */
      *x = NULL;	/* GC's the object */
    }

    *x = next_to_finalize();
  }

  unprotect(x);
  freemem(x);
}

PRIVATE void run_main_loop(void) {
  int idle_loops = 0;

  while (count_active_threads() > 0) {
    run_blocked_queue();	/* try to resume blocked threads */

    if (run_run_queue())	/* run runnable threads for a quantum each */
      idle_loops = 0;
    else
      idle_loops++;

    if (idle_loops >= 5) {	/* If we've been idle for a while... */
      gc();				/* ... garbage collect ... */
      run_finalize_queue();		/* ... clear up destruction of garbage ... */
      block_until_event();		/* ... and wait for something to happen. */
      idle_loops = 0;
    }
  }
}

PRIVATE void siginthandler(int dummy) {
  printf("\nSIGINT CAUGHT\n");
  fflush(stdout);
  exit(MOVE_EXIT_INTERRUPTED);
}

PRIVATE void import_db(char *filename, int load_threads) {
  FILE *f = fopen(filename, "r");

  if (f == NULL)
    return;

  vm_restore_from(f, load_threads);
  fclose(f);
}

PRIVATE void import_cmdline_files(int argc, char *argv[]) {
  int i;
  FILE *ci, *co;

  co = stdout;

  for (i = 0; i < argc; i++) {
    if (!strcmp(argv[i], "-")) {
      printf("listening on stdin\n");
      ci = stdin;
    } else {
      ci = fopen(argv[i], "r");

      if (ci == NULL)
	continue;

      printf("importing %s\n", argv[i]);
    }

    compile_main(ci, co);
  }
}

PRIVATE void write_pid(void) {
  FILE *f = fopen("move.pid", "w");

  if (f == NULL)
    return;

  fprintf(f, "%ld\n", (long) getpid());
  fclose(f);
}

PUBLIC int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr,
	    "3-MOVE (%d-bit mode; main.o compiled on " __DATE__ " at " __TIME__ ")\n"
	    "Usage: move [-t] <dbfilename> [<move-source-code-file> ...]\n"
	    "\t-t\tInhibits loading threads which were active when the DB was saved\n",
	    (int) sizeof(word) * 8);
    exit(MOVE_EXIT_ERROR);
  }

  signal(SIGINT, siginthandler);	/* %%% This can be made to emergency-flush
					   the database to disk, later on %%% */

  write_pid();

  init_gc();
  init_object();
  init_prim();
  init_vm_global();
  init_thread();

  checkpoint_filename = "move.checkpoint";

  install_primitives();

  {
    int load_threads = 1;

    if (!strcmp(argv[1], "-t")) {
      load_threads = 0;
      argv++;
      argc--;
    }

    import_db(argv[1], load_threads);
  }

  bind_primitives_to_symbols();

  import_cmdline_files(argc - 2, argv + 2);

  run_main_loop();

  done_gc();
  return MOVE_EXIT_OK;
}
