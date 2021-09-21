MODE=DEBUG

CC=gcc
CFLAGS=-pedantic -Wall -Wextra -g \
	   -D ${MODE}=1
LFLAGS=-lpthread

TARGET=maulwurf
OFILES=main.o index.o interactive.o commands.o file_io.o program_args.o

maulwurf: ${OFILES}
	${CC} -o ${TARGET} ${OFILES} ${LFLAGS}

main.o: main.c
	${CC} -o main.o -c main.c ${CFLAGS}

index.o: index.c
	${CC} -o index.o -c index.c ${CFLAGS}

interactive.o: interactive.o
	${CC} -o interactive.o -c interactive.c ${CFLAGS}

commands.o: commands.o
	${CC} -o commands.o -c commands.c ${CFLAGS}

file_io.o: file_io.o
	${CC} -o file_io.o -c file_io.c ${CFLAGS}

program_args.o: program_args.o
	${CC} -o program_args.o -c program_args.c ${CFLAGS}

.PHONY: clean

clean:
	-rm -f ${TARGET} ${OFILES}
