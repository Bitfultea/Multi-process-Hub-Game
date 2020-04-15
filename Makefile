CC=gcc
CFLAGS=-std=gnu99 -Wall -pedantic

all: 2310alice 2310bob 2310hub

util.o: util.c util.h
	$(CC) $(CFLAGS) -c util.c -o util.o

player.o: player.c player.h
	$(CC) $(CFLAGS) -c player.c -o player.o

2310alice: player.o util.o alice.c
	$(CC) $(CFLAGS) player.o util.o alice.c -o 2310alice

2310bob: player.o util.o bob.c
	$(CC) $(CFLAGS) player.o util.o bob.c -o 2310bob

2310hub: hub.c util.o
	$(CC) $(CFLAGS) hub.c util.o -o 2310hub

clean:
	rm -rf util.o player.o 2310alice 2310bob 2310hub
