OBJS = main.o bmp.o
CC = gcc
CFLAGS = -Wall -Wextra --pedantic-errors -std=c89 -c
LFLAGS = -lncurses

bmp.o: bmp.c bmp.h
	$(CC) $(CFLAGS) bmp.c

main.o: main.c bmp.h
	$(CC) $(CFLAGS) main.c

slide: $(OBJS)
	$(CC) $(OBJS) $(LFLAGS) -o slide

all: slide

clean:
	rm $(OBJS)
	rm slide
