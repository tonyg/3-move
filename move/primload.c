#include "global.h"
#include "object.h"
#include "vm.h"
#include "prim.h"
#include "primload.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

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
    module_booter();
  }

  fclose(f);
}
