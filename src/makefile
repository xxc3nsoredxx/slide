OBJS = main.o bmp.o
CC = gcc
CFLAGS = -Wall -Wextra --pedantic-errors -std=c89 -c

bmp.o: bmp.c bmp.h
	$(CC) $(CFLAGS) bmp.c

main.o: main.c bmp.h
	$(CC) $(CFLAGS) main.c

ss: $(OBJS)
	$(CC) $(OBJS) -o ss

all: ss
