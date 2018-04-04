OBJS = main.o
CC = gcc
CFLAGS = -Wall -Wextra --pedantic-errors -std=c89 -c
LFLAGS = -lncurses -lpng

main.o: main.c
	$(CC) $(CFLAGS) main.c

slide: $(OBJS)
	$(CC) $(OBJS) $(LFLAGS) -o slide

all: slide

clean:
	rm $(OBJS)
	rm slide
