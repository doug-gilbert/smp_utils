SHELL = /bin/sh
KERNELRELEASE ?= $(shell uname -r)
KSRC ?= /lib/modules/$(KERNELRELEASE)/build

PREFIX=/usr/local
INSTDIR=$(DESTDIR)/$(PREFIX)/bin
MANDIR=$(DESTDIR)/$(PREFIX)/share/man

ifndef CC
CC = gcc
endif
LD = $(CC)

EXECS = smp_sgv4_io.o

OS_FLAGS = -DSMP_UTILS_LINUX
LARGE_FILE_FLAGS = -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
EXTRA_FLAGS = $(OS_FLAGS) $(LARGE_FILE_FLAGS)

INCLUDES = -I ../include -I $(KSRC)/include -idirafter ../include/dummy

# CFLAGS =  -O2 -Wall -W $(EXTRA_FLAGS)
CFLAGS = -g -O2 -Wall -W $(EXTRA_FLAGS)
# CFLAGS = -O2 -Wall -W -pedantic -std=c99 $(EXTRA_FLAGS)

LDFLAGS = 
# LDFLAGS = -v -lm

.c.o:
	$(CC) $(INCLUDES) $(CFLAGS) -c -o $@ $<

all: $(EXECS)

depend dep:
	for i in *.c; do $(CC) $(INCLUDES) $(CFLAGS) -M $$i; \
	done > .depend

clean:
	/bin/rm -f *.o $(EXECS) core* .depend *.a *.la *.lo
	/bin/rm -rf .libs

install: $(EXECS)
	install -d $(INSTDIR)
	for name in $^; \
	 do install -s -o root -g root -m 755 $$name $(INSTDIR); \
	done
	install -d $(MANDIR)/$(MAN_PREF)
	for mp in $(MAN_PGS); \
	 do install -o root -g root -m 644 $$mp $(MANDIR)/$(MAN_PREF); \
	 gzip -9f $(MANDIR)/$(MAN_PREF)/$$mp; \
	done

uninstall:
	dists="$(EXECS)"; \
	for name in $$dists; do \
	 rm -f $(INSTDIR)/$$name; \
	done
	for mp in $(MAN_PGS); do \
	 rm -f $(MANDIR)/$(MAN_PREF)/$$mp.gz; \
	done

ifeq (.depend,$(wildcard .depend))
include .depend
endif
