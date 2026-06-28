CC = gcc

HSHOBJ = hsh.o memento.o
DEPFILES = hsh.d memento.d

CFLAGS = -Wall -Wextra -MD -MP

all: hsh

hsh: $(HSHOBJ)
	$(CC) -o hsh $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f hsh $(HSHOBJ) $(DEPFILES)

-include $(DEPFILES)

.PHONY: clean
