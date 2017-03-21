#!/usr/bin/make -f

all: chinit
	sudo chown root:root chinit
	sudo chmod 4755 chinit

chinit: chinit.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	-rm -f chinit
