# Makefile for C projects, derived from the makefile for MOOF.
# Copyright (C) 1994 through 1997, Tony Garnock-Jones (tonyg@kcbbs.gen.nz)
#
# To customise, set -
#	TARGET		to the name of the desired executable file.
#	objects		to the names of all the object files in the project.
#
# Select the LIBS definition you want.

TARGET = move
objects = buffer.o conn.o gc.o hashtable.o main.o method.o \
	object.o parser.o perms.o persist.o prim.o primload.o \
	recmutex.o scanner.o slot.o thread.o vm.o \
	\
	PRIM.io.o PRIM.compound.o PRIM.system.o PRIM.misc.o PRIM.object.o

LFLAGS = -o $(TARGET) # -g
DELETE = rm -f

######################### UNCOMMENT FOR LINUX
CC = gcc
CFLAGS = -D_REENTRANT -c -Wall -g -O2
LIBS =

######################### UNCOMMENT FOR OSF/1 ON ALPHA
##CC = cc
##CFLAGS = -D_THREAD_SAFE -c -g # -O2
##LIBS = -lpthread -lmach -lexc -lc

######################### You shouldn't need to change below this line.

all: $(TARGET)

fullclean: clean
	$(DELETE) $(TARGET)

clean:
	$(DELETE) $(objects) *~

distrib: fullclean
	(pushd .. ; \
	 tar --exclude RCS --exclude OOM --exclude .emacs.desktop --exclude .gdbinit \
		-cvf move.tar move ; \
	 popd )
	gzip ../move.tar

$(TARGET): $(objects) Makefile
	$(CC) $(LFLAGS) $(objects) $(LIBS)
#	strip $(TARGET)

.c.o:
	$(CC) $(CFLAGS) $<
