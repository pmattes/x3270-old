# Copyright 1999, 2000 by Paul Mattes.
#  Permission to use, copy, modify, and distribute this software and its
#  documentation for any purpose and without fee is hereby granted,
#  provided that the above copyright notice appear in all copies and that
#  both that copyright notice and this permission notice appear in
#  supporting documentation.

# Makefile for pr3287

OBJECTS = pr3287.o ctlr.o trace_ds.o tables.o telnet.o sf.o
#CDEBUGFLAGS = -g -Wall
CFLAGS = $(INCLUDES) $(DEFINES) $(CDEBUGFLAGS)

all: pr3287

version.o: version.txt mkversion.sh
	@chmod +x mkversion.sh version.txt
	sh ./mkversion.sh $(CC) pr3287

pr3287: $(OBJECTS) version.o
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) version.o

clean:
	rm -f pr3287 $(OBJECTS) version.o
