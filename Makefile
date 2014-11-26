CC = icc
CFLAGS = -g -O3 -Wall -xHost -fno-alias -std=c99

OBJS = main.o search.o eval.o data.o board.o book.o

chess: ${OBJS}
	$(CC) $(CFLAGS) -o $@ $^

.c.o:
	$(CC) -c $(CFLAGS) $<

clean:
	rm -f chess *.o 