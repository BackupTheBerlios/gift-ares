CFLAGS=-g -Wall -I../lib
#CFLAGS=-O2 -Wall -I../lib

LDFLAGS=-L../lib -lares -levent -lm -lz

OBJECTS= \
	main.o \
	cmd.o \
	testing.o

all: ares

ares: $(OBJECTS) ../lib/libares.a
	$(CC) $+ -o $@ $(LDFLAGS)

clean:
	rm -f ares $(OBJECTS)
