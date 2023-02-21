
#
#dbx cannot handle -O for optimization reset -g and set -O after tests
#

SRC = 		access.c \
		btree.c \
		cat.c \
		create.c \
		err.c \
		hash.c \
		iex.c \
		impl.c \
		io.c \
		io_lib.c \
		lexan.c \
		misc.c \
		parser.c \
		pgm_int.c \
		quedcmp.c \
                requ.c \
		tabl.c \
		view.c \
		xcom.c \
		xinterp.c

OBJ = 		access.o \
		btree.o \
		cat.o \
		create.o \
		err.o \
		hash.o \
		iex.o \
		impl.o \
		io.o \
		io_lib.o \
		lexan.o \
		misc.o \
		parser.o \
		pgm_int.o \
		quedcmp.o \
                requ.o \
		tabl.o \
		view.o \
		xcom.o \
		xinterp.o

HDR =		btree.h \
		requ.h 


CFLAGS =  -g -DUNIX -DO_BINARY=0 

CC     =  cc
all: requiem


requiem: lib requ.o 
	${CC} $(CFLAGS) -o requiem ${SRC}


lib: ${OBJ}


lint:
	lint -bu ${SRC} requ.c

btree.o  create.o io.o parser.o varsbtr.o: requ.h btree.h
cat.o create.o err.o fifo.o hash.o iex.o impl.o io_lib.o lexan.o misc.o: requ.h
pgm_int.o quedcmp.o requ.o tabl.o view.o xcom.o xinterp.o: requ.h
tmm.o: requ.h tmm.h



