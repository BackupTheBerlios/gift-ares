CFLAGS=-g -Wall -I../lib

LDFLAGS=-L../lib -lares -levent

OBJECTS= \
	main.o \
	cmd.o \
	testing.o

all: ares

ares: $(OBJECTS)
	$(CC) $+ -o $@ $(LDFLAGS)

clean:
	rm -f ares $(OBJECTS)
