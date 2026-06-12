CC = gcc
CFLAGS = -Wall -Wextra

all: hsh

hsh: hsh.o
	$(CC) -o hsh hsh.o

hsh.o: hsh.c
	$(CC) $(CFLAGS) -c -o hsh.o hsh.c

clean:
	rm -f hsh hsh.o

.PHONY: clean
