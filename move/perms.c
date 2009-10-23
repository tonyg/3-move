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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "global.h"
#include "object.h"
#include "perms.h"

PUBLIC int in_group(OBJECT what, VECTOR group) {
  int i;

  if (group == NULL)
    return 0;

  for (i = 0; i < group->_.length; i++) {
    OBJ x = AT(group, i);

    if (x == (OBJ) what)
      return 1;

    if (VECTORP(x))
      if (in_group(what, (VECTOR) x))
	return 1;
  }

  return 0;
}
