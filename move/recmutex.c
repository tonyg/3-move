#include "global.h"
#include "config.h"
#include "recmutex.h"
#include "object.h"
#include "vm.h"
#include "thread.h"

PUBLIC int recmutex_init(recmutex_t *mutex) {
  mutex->counter = 0;
  mutex->owner_threadnum = current_thread ? current_thread->number : 0;
  return 0;
}

PUBLIC int recmutex_lock(recmutex_t *mutex) {
  int self = current_thread ? current_thread->number : 0;

  if (mutex->counter != 0 && mutex->owner_threadnum != self)
    return -1;	/* EAGAIN, more or less */

  mutex->counter++;
  mutex->owner_threadnum = self;
  return 0;
}

PUBLIC int recmutex_unlock(recmutex_t *mutex) {
  if (--mutex->counter <= 0) {
    mutex->counter = 0;
    mutex->owner_threadnum = 0;
  }
  return 0;
}
