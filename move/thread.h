#ifndef Thread_H
#define Thread_H

typedef struct Thread {
  pthread_t tid;
  VMSTATE vms;
  int number;
  struct Thread *next;
} Thread, *THREAD;

extern void init_thread(void);

extern THREAD find_thread_by_tid(pthread_t tid);
extern THREAD find_thread_by_number(int number);
extern THREAD find_thread_by_vms(VMSTATE vms);

extern THREAD begin_thread(OVECTOR closure, VMSTATE parentvms);

extern void load_restartable_threads(void *phandle, FILE *f);
extern void save_restartable_threads(void *phandle, FILE *f);

#endif
