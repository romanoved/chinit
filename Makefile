#!/usr/bin/make -f

all: chinit
	sudo chown root:root chinit
	sudo chmod 4755 chinit

chinit: chinit.c
	$(CC) $(CFLAGS) $(LDFLAGS) -Wall -pedantic -o $@ $<

clean:
	-rm -f chinit
