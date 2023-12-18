CFLAGS=-std=c11 -g -fno-common
CC=clang
SRCS=$(wildcard *.c)
OBJS=${SRCS:.c=.o}
#OBJS=${SRCS:*.c=*.o}

rvcc:${OBJS}
	$(CC) ${CFLAGS} -o $@ $^ ${LDFLAGS}

${OBJS}:rvcc.h

test: rvcc
	./test.sh

clean:
	rm -f rvcc *.o *.s tmp* a.out

PHONY: test clean