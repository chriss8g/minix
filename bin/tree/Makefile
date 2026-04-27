# Makefile para tree

CC = cc
CFLAGS = -D_MINIX -D_POSIX_SOURCE
BINDIR = /usr/bin
PROG = tree

all: ${PROG}

${PROG}: ${PROG}.c
	${CC} ${CFLAGS} -o ${PROG} ${PROG}.c

install: ${PROG}
	install -cs -o root ${PROG} ${BINDIR}

clean:
	rm -f ${PROG}
