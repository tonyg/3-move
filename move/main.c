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

typedef struct repl_data {
  OVECTOR input, output;
  VMREGS vmregs;
  OBJ holder;
} repl_data, *REPL_DATA;

PRIVATE void *repl(void *arg) {
  VECTOR args = (VECTOR) arg;
  REPL_DATA rd = allocmem(sizeof(repl_data));
  VMstate vms;

  rd->input = (OVECTOR) AT(args, 0);
  rd->output = (OVECTOR) AT(args, 1);
  rd->holder = NULL;

  protect((OBJ *)(&rd->input));
  protect((OBJ *)(&rd->output));

  pthread_mutex_lock(&boot_mutex);
  pthread_cond_broadcast(&boot_signal);
  pthread_mutex_unlock(&boot_mutex);

  protect(&rd->holder);

  gc_inc_safepoints();
  rd->vmregs = (VMREGS) newvector(NUM_VMREGS);	/* dodgy casting :-) */
  vms.r = rd->vmregs;
  protect((OBJ *)(&rd->vmregs));

  init_vm(&vms);
  vms.r->vm_input = rd->input;
  vms.r->vm_output = rd->output;

  while (vms.c.vm_state == 1) {
    ScanInst si;
    char buf[256];

    rd->holder = (OBJ) newbvector(0);
    gc_dec_safepoints();

    while (1) {
      if (conn_gets(buf, 256, rd->input) == NULL)
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

      rd->holder = (OBJ) bvector_concat((BVECTOR) rd->holder, newstring(buf));
    }

    gc_inc_safepoints();

    if (conn_closed(rd->input)) {
      break;
    }

    fill_scaninst(&si, newstringconn((BVECTOR) rd->holder));
    rd->holder = (OBJ) parse(&vms, &si);
    gc_reach_safepoint();

    if (rd->holder == NULL) {
      sprintf(buf, "-->! the compiler returned an error condition.\n");
    } else {
      gc_dec_safepoints();
      vms.r->vm_uid = vms.r->vm_effuid = NULL;
      run_vm(&vms, NULL, (OVECTOR) rd->holder);
      gc_inc_safepoints();

      rd->holder = (OBJ) newvector(2);
      ATPUT((VECTOR) rd->holder, 0, NULL);
      ATPUT((VECTOR) rd->holder, 1, vms.r->vm_acc);
      rd->holder = lookup_prim(0x00001)(&vms, (VECTOR) rd->holder);
      rd->holder = (OBJ) bvector_concat((BVECTOR) rd->holder, newbvector(1));
      	/* terminates C-string */
      gc_reach_safepoint();

      sprintf(buf, "--> %s\n", ((BVECTOR) rd->holder)->vec);
    }

    conn_puts(buf, rd->output);
  }

  conn_close(rd->input);
  conn_close(rd->output);

  gc_dec_safepoints();

  unprotect((OBJ *)(&rd->vmregs));
  unprotect(&rd->holder);
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

  pthread_create(&newthr, NULL, repl, (void *) args);
  pthread_detach(newthr);

  pthread_cond_wait(&boot_signal, &boot_mutex);
  pthread_mutex_unlock(&boot_mutex);
}

PRIVATE void *listener(void *arg) {
  int server;
  struct sockaddr_in addr;
  int addrlen;
  int pnum = 7777;

  server = socket(AF_INET, SOCK_STREAM, 0);

  if (server == -1)
    return NULL;

  while (1) {
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(pnum);

    if (bind(server, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
      pnum += 1111;
      continue;
    }

    printf("bound %d\n", pnum);
    break;
  }

  if (listen(server, 5) == -1)
    return NULL;

  {
    OVECTOR ci, co;

    printf("listening on stdin\n");

    gc_inc_safepoints();
    ci = newfileconn(0);	/* stdin */
    co = newfileconn(1);	/* stdout */
    start_connection(ci, co);
    gc_dec_safepoints();
  }

  while (1) {
    int fd = accept(server, (struct sockaddr *) &addr, &addrlen);
    OVECTOR conn;

    printf("accepted %d\n", fd);

    if (fd == -1)
      return NULL;

    gc_inc_safepoints();
    conn = newfileconn(fd);
    start_connection(conn, conn);
    gc_dec_safepoints();
  }

  return NULL;
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

PUBLIC int main(int argc, char *argv[]) {
  pthread_t gc_thread, listener_thread, finalizer_thread;

  signal(SIGINT, siginthandler);

  pthread_mutex_init(&boot_mutex, NULL);
  pthread_cond_init(&boot_signal, NULL);

  init_gc();
  init_object();
  init_prim();
  init_vm_global();

  install_primitives();

  pthread_create(&gc_thread, NULL, vm_gc_thread_main, NULL);
  pthread_create(&listener_thread, NULL, listener, NULL);
  pthread_create(&finalizer_thread, NULL, finalizer, NULL);

  pthread_join(listener_thread, NULL);
  finalizer_quit = 1;
  awaken_finalizer();
  make_gc_thread_exit();
  pthread_join(gc_thread, NULL);
  pthread_join(finalizer_thread, NULL);

  done_gc();
  return 0;
}
