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
