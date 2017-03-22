#!/usr/bin/make -f

DESTDIR=/

.PHONY: all
all: ;

build: chinit

install: build
	mkdir -p $(DESTDIR)/usr/sbin/
	cp chinit $(DESTDIR)/usr/sbin/
	chmod 4755 $(DESTDIR)/usr/sbin/chinit
	ls -lah $(DESTDIR)/usr/sbin/chinit

test: build
	-sudo ./chinit_test # actually, you can skip this

clean:
	-rm -f chinit

chinit: chinit.c
	$(CC) $(CFLAGS) $(LDFLAGS) -Wall -pedantic -o $@ $<
