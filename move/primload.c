#include "global.h"
#include "object.h"
#include "vm.h"
#include "prim.h"
#include "primload.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

#define USE_EXTERNAL_PRIMITIVE_MODULES 0

#if !USE_EXTERNAL_PRIMITIVE_MODULES

extern void install_PRIM_io(void);
extern void install_PRIM_compound(void);
extern void install_PRIM_system(void);
extern void install_PRIM_misc(void);
extern void install_PRIM_object(void);

PUBLIC void install_primitives(void) {
  install_PRIM_io();
  install_PRIM_compound();
  install_PRIM_system();
  install_PRIM_misc();
  install_PRIM_object();
}

#else

PUBLIC void install_primitives(void) {
  FILE *f = fopen(INDEX_FILE, "r");
  
  while (!feof(f)) {
    char n[80];
    void *dl_handle;
    void (*module_booter)(void);
    
    if (!fgets(n, 80, f))
      break;

    while (strlen(n) > 0 &&
	   (n[strlen(n)-1] == '\n' ||
	    n[strlen(n)-1] == '\r'))
      n[strlen(n)-1] = '\0';	/* remove trailing whitespace */

    if (strlen(n) == 0)
      continue;

    dl_handle = dlopen(n, RTLD_NOW);

    if (dl_handle == NULL)
      continue;

    module_booter = dlsym(dl_handle, "installer");
    if (module_booter != NULL)
      module_booter();
  }

  fclose(f);
}

#endif
