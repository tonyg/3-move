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

#include <dirent.h>

#define MINDELAY 10

static char *prefix;
int prefixlen;

static int selector(const struct dirent *d) {
  return !strncmp(d->d_name, prefix, prefixlen);
}

static int sorter(const struct dirent **a, const struct dirent **b) {
  int ai, bi;

  ai = atoi((*a)->d_name + prefixlen);
  bi = atoi((*b)->d_name + prefixlen);
  return ai - bi;
}

static void cleanup(int keepnum) {
  struct dirent **list;
  int i, n;

  n = scandir(".", &list, selector, sorter);

  if (n > keepnum) {
    for (i = 0; i < n - keepnum; i++) {
      char buf[1024];
      sprintf(buf, "./%s", list[i]->d_name);
      remove(buf);
    }
  }

  for (i = 0; i < n; i++)
    free(list[i]);
  free(list);
}

static void write_pid(void) {	
  FILE *f = fopen("checkpoint-cleanup.pid", "w");

  if (f == NULL)
    return;

  fprintf(f, "%ld\n", (long) getpid());
  fclose(f);
}

int main(int argc, char **argv) {
  int delay;
  int keepnum;

  if (argc<3) {
    fprintf(stderr,
	    "Usage: checkpoint-cleanup <filename-prefix> <time-between-cleans>\n"
	    "                          <how-many-to-keep>\n");
    exit(1);
  }

  prefix = argv[1];			/* GLOBAL VARIABLES! */
  prefixlen = strlen(prefix);

  delay = atoi(argv[2]);
  keepnum = atoi(argv[3]);

  if (delay < MINDELAY) {
    fprintf(stderr, "Delay must be >= %d seconds.\n", MINDELAY);
    exit(1);
  }

  if (keepnum < 1) {
    fprintf(stderr, "Number of files to keep must be 1 or greater.\n");
    exit(1);
  }

  write_pid();

  while (1) {
    cleanup(keepnum);
    sleep(delay);
  }

  return 0;
}
