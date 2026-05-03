# Makefile for SmartMemEngine userspace tools

CC = gcc
CFLAGS = -Wall -Wextra -O2

TARGETS = memctl memstat memview

all: $(TARGETS)

memctl: memctl.c
	$(CC) $(CFLAGS) -o $@ $<

memstat: memstat.c
	$(CC) $(CFLAGS) -o $@ $<

memview: memview.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGETS)

install: all
	install -d /usr/local/bin
	install -m 755 memctl /usr/local/bin/
	install -m 755 memstat /usr/local/bin/
	install -m 755 memview /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/memctl
	rm -f /usr/local/bin/memstat
	rm -f /usr/local/bin/memview

.PHONY: all clean install uninstall
