CFLAGS= -g3 -O0 -Wall -DDEBUG -I../lib #-DHEAVY_DEBUG
LDFLAGS=-L../lib -lgift -lgiftproto -laresgift -lz -lm

OBJECTS = \
	as_gift_plugin.o

#libAres.la: $(OBJECTS)
#	libtool --mode=link gcc -module -avoid-version $(CFLAGS) $(LDFLAGS) as_plugin.c $(OBJECTS) -o $@

libAres.so: $(OBJECTS) ../lib/libaresgift.a
	gcc -shared $(OBJECTS) $(LDFLAGS) -o $@

all: libAres.so
