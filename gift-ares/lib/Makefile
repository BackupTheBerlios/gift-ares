CFLAGS=-g -Wall -DDEBUG

OBJECTS = \
	as_ares.o \
	as_crypt.o \
	as_event.o \
	as_list.o \
	as_hashtable.o \
	as_log.o \
	as_packet.o \
	as_search.o \
	as_session.o \
	as_session_man.o \
	as_node.o \
	as_node_man.o \
	as_tcp.o \
	as_tokenize.o \
	as_sha1.o

all: libares.a($(OBJECTS))

clean:
	rm -f libares.a $(OBJECTS)