# Makefile for MOVE
# Copyright (C) 1998, Tony Garnock-Jones (tonyg@kcbbs.gen.nz)
#
# To customise, select the set of definitions below as appropriate for your
# operating system and machine architecture.

TARGET = move
objects = buffer.o conn.o gc.o hashtable.o main.o method.o \
	object.o parser.o perms.o persist.o prim.o primload.o \
	recmutex.o scanner.o slot.o thread.o vm.o \
	\
	PRIM.io.o PRIM.compound.o PRIM.system.o PRIM.misc.o PRIM.object.o

LFLAGS = -o $(TARGET) # -g
DELETE = rm -f

######################### UNCOMMENT FOR LINUX ON x86
CC = gcc
CFLAGS = -c -Wall -g -O2
LIBS =

######################### UNCOMMENT FOR OSF/1 ON ALPHA
##CC = cc
##CFLAGS = -D_THREAD_SAFE -c -g # -O2
##LIBS = -lpthread -lmach -lexc -lc

######################### UNCOMMENT FOR SunOS 5.5.1 ON SPARC
##CC = gcc
##CFLAGS = -c -g -O2
##LIBS = -lsocket -lnsl

######################### You shouldn't need to change below this line.

all: $(TARGET)

clean:
	$(DELETE) $(TARGET)
	$(DELETE) $(objects) *~

distrib: clean
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
