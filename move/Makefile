# Makefile for C projects, derived from the makefile for MOOF.
# Copyright (C) 1994 through 1997, Tony Garnock-Jones (tonyg@kcbbs.gen.nz)
#
# To customise, set -
#	TARGET		to the name of the desired executable file.
#	objects		to the names of all the object files in the project.
#
# Select the LIBS definition you want.

INDEX_FILE = index.prim

TARGET = move
objects = barrier.o buffer.o conn.o gc.o hashtable.o main.o method.o \
	object.o parser.o perms.o prim.o primload.o recmutex.o scanner.o \
	slot.o vm.o
prims = $(shell cat $(INDEX_FILE))

CC = gcc
CFLAGS = -D_REENTRANT -DINDEX_FILE=\"$(INDEX_FILE)\" -c -Wall -g # -O2
LFLAGS = -rdynamic -o $(TARGET) # -g
DELETE = rm -f

#### Libraries for math and threading support. If you use sockets under EMX with OS/2,
#### you will need -lsocket as well. Also supports dynamic linking under Linux.
LIBS = -lm -lpthread -ldl

all: $(TARGET) $(prims)

clean:
	$(DELETE) *.o *~

distrib: clean
	$(DELETE) $(TARGET)
	$(DELETE) $(prims)

$(TARGET): $(objects) Makefile
	$(CC) $(LFLAGS) $(objects) $(LIBS)
#	strip $(TARGET)

%.so: %.o
	ld -o $@ -shared $<

.c.o:
	$(CC) $(CFLAGS) $<
