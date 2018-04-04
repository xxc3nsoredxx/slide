# OBJS = main.o bmp.o
OBJS = main.o
CC = gcc
CFLAGS = -Wall -Wextra --pedantic-errors -std=c89 -c
LFLAGS = -lncurses -lpng

# bmp.o: bmp.c bmp.h
# 	$(CC) $(CFLAGS) bmp.c

# main.o: main.c bmp.h
main.o: main.c
	$(CC) $(CFLAGS) main.c

slide: $(OBJS)
	$(CC) $(OBJS) $(LFLAGS) -o slide

all: slide

clean:
	rm $(OBJS)
	rm slide
