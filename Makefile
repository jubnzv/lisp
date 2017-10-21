CC=gcc
CFLAGS=-Wall -lm -ledit

all:
	${CC} ${CFLAGS} lisp.c mpc/mpc.c -o lisp

clean:
	rm -rf *.o lisp
