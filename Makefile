CC=gcc
CFLAGS=-Wall -lm -ledit -DL_DEBUG=1

all:
	${CC} ${CFLAGS} lisp.c mpc/mpc.c -o lisp

clean:
	rm -rf *.o lisp
