#include "global.h"
#include "object.h"
#include "vm.h"
#include "prim.h"
#include "barrier.h"
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

#include <pthread.h>

PRIVATE pthread_mutex_t boot_mutex;
PRIVATE pthread_cond_t boot_signal;

PRIVATE pthread_mutex_t quit_mutex;
PRIVATE pthread_cond_t quit_signal;

typedef struct repl_data {
  OVECTOR input, output;
  VMREGS vmregs;
  OBJ h1, h2;
} repl_data, *REPL_DATA;

PRIVATE void *compile_main(void *arg) {
  VECTOR args = (VECTOR) arg;
  REPL_DATA rd = allocmem(sizeof(repl_data));
  VMstate vms;

  rd->input = (OVECTOR) AT(args, 0);
  rd->output = (OVECTOR) AT(args, 1);
  rd->h1 = rd->h2 = NULL;

  protect((OBJ *)(&rd->input));
  protect((OBJ *)(&rd->output));

  pthread_mutex_lock(&boot_mutex);
  pthread_cond_broadcast(&boot_signal);
  pthread_mutex_unlock(&boot_mutex);

  protect(&rd->h1);
  protect(&rd->h2);

  gc_inc_safepoints();
  rd->vmregs = (VMREGS) newvector(NUM_VMREGS);	/* dodgy casting :-) */
  vms.r = rd->vmregs;
  protect((OBJ *)(&rd->vmregs));

  init_vm(&vms);
  vms.r->vm_input = rd->input;
  vms.r->vm_output = rd->output;

  while (vms.c.vm_state != VM_STATE_DYING) {
    ScanInst si;
    char buf[16384];

    rd->h1 = (OBJ) newbvector(0);

    while (1) {
      char *result;

      gc_dec_safepoints();
      result = conn_gets(buf, 256, rd->input);
      gc_inc_safepoints();

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
      gc_reach_safepoint();
    }

    if (conn_closed(rd->input)) {
      printf("%ld CONN CLOSED\n", pthread_self());
      break;
    }

    rd->h2 = (OBJ) newstringconn((BVECTOR) rd->h1);
    fill_scaninst(&si, (OVECTOR) rd->h2);

    while (!conn_closed((OVECTOR) rd->h2) && vms.c.vm_state != VM_STATE_DYING) {
      rd->h1 = (OBJ) parse(&vms, &si);
      gc_reach_safepoint();

      if (rd->h1 == NULL) {
	sprintf(buf, "%ld -->! the compiler returned NULL.\n", pthread_self());
      } else {
	ATPUT((OVECTOR) rd->h1, ME_OWNER, (OBJ) vms.r->vm_uid);
	vms.r->vm_effuid = vms.r->vm_uid;
	{
	  OVECTOR c = newovector_noinit(CL_MAXSLOTINDEX, T_CLOSURE);
	  ATPUT(c, CL_SELF, NULL);
	  ATPUT(c, CL_METHOD, rd->h1);
	  rd->h1 = (OBJ) c;
	}
	apply_closure(&vms, (OVECTOR) rd->h1, newvector_noinit(1));

	gc_dec_safepoints();
	run_vm(&vms);
	gc_inc_safepoints();

	rd->h1 = (OBJ) newvector(2);
	ATPUT((VECTOR) rd->h1, 0, NULL);
	ATPUT((VECTOR) rd->h1, 1, vms.r->vm_acc);
	rd->h1 = lookup_prim(0x00001)(&vms, (VECTOR) rd->h1);
	rd->h1 = (OBJ) bvector_concat((BVECTOR) rd->h1, newbvector(1));
      	/* terminates C-string */

	gc_reach_safepoint();

	sprintf(buf, "%ld --> %s\n", pthread_self(), ((BVECTOR) rd->h1)->vec);
      }

      conn_puts(buf, rd->output);
    }
  }

  gc_dec_safepoints();

  unprotect((OBJ *)(&rd->vmregs));
  unprotect(&rd->h2);
  unprotect(&rd->h1);
  unprotect((OBJ *)(&rd->output));
  unprotect((OBJ *)(&rd->input));

  freemem(rd);
  return NULL;
}

PRIVATE void start_connection(OVECTOR conni, OVECTOR conno) {
  VECTOR args = newvector_noinit(2);
  pthread_t newthr;

  ATPUT(args, 0, (OBJ) conni);
  ATPUT(args, 1, (OBJ) conno);

  pthread_mutex_lock(&boot_mutex);

  pthread_create(&newthr, NULL, compile_main, (void *) args);
  pthread_detach(newthr);

  pthread_cond_wait(&boot_signal, &boot_mutex);
  pthread_mutex_unlock(&boot_mutex);
}

PRIVATE int finalizer_quit = 0;

PRIVATE void *finalizer(void *arg) {
  OBJ *x;

  x = allocmem(sizeof(OBJ));
  *x = NULL;

  protect(x);

  while (!finalizer_quit) {
    wait_for_finalize();

    for (*x = next_to_finalize(); *x != NULL; *x = next_to_finalize()) {
      if (OVECTORP(*x)) {
	finalize_ovector((OVECTOR) (*x));
      } else {
	/* %%% NOTE here's where objects should be handed to user level
	   to finalize. %%% */
	*x = NULL;	/* GC's the object */
      }
    }
  }

  unprotect(x);
  freemem(x);

  return NULL;
}

PRIVATE void siginthandler(int dummy) {
  printf("\nSIGINT CAUGHT\n");
  fflush(stdout);
  exit(123);
}

PRIVATE void import_db(char *filename) {
  FILE *f = fopen(filename, "r");

  if (f == NULL)
    return;

  vm_restore_from(f);
  fclose(f);
}

PRIVATE void import_cmdline_files(int argc, char *argv[]) {
  int i;
  OVECTOR ci, co;

  gc_inc_safepoints();
  co = newfileconn(1);

  for (i = 0; i < argc; i++) {
    if (!strcmp(argv[i], "-")) {
      printf("listening on stdin\n");
      ci = newfileconn(0);
    } else {
      int fd = open(argv[i], O_RDONLY);

      if (fd == -1)
	continue;

      printf("importing %s %d\n", argv[i], fd);
      ci = newfileconn(fd);
    }

    start_connection(ci, co);
  }

  gc_dec_safepoints();
}

PUBLIC int main(int argc, char *argv[]) {
  pthread_t gc_thread, finalizer_thread;

  if (argc < 2) {
    fprintf(stderr,
	    "Usage: move <dbfilename> [<move-source-code-file> ...]\n");
    exit(1);
  }

  signal(SIGINT, siginthandler);	/* %%% This can be made to emergency-flush
					   the database to disk, later on %%% */

  pthread_mutex_init(&boot_mutex, NULL);
  pthread_cond_init(&boot_signal, NULL);

  pthread_mutex_init(&quit_mutex, NULL);
  pthread_cond_init(&quit_signal, NULL);

  init_gc();
  init_object();
  init_prim();
  init_vm_global();
  init_thread();

  checkpoint_filename = "move.checkpoint";

  pthread_create(&gc_thread, NULL, vm_gc_thread_main, NULL);
  pthread_create(&finalizer_thread, NULL, finalizer, NULL);

  import_db(argv[1]);
  install_primitives();

  import_cmdline_files(argc - 2, argv + 2);

  pthread_mutex_lock(&quit_mutex);
  pthread_cond_wait(&quit_signal, &quit_mutex);
  pthread_mutex_unlock(&quit_mutex);
  
  finalizer_quit = 1;
  awaken_finalizer();
  make_gc_thread_exit();

  pthread_join(gc_thread, NULL);
  pthread_join(finalizer_thread, NULL);

  done_gc();
  return 0;
}
