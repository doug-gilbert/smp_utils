SHELL = /bin/sh

PREFIX=/usr/local
INSTDIR=$(DESTDIR)/$(PREFIX)/bin
MANDIR=$(DESTDIR)/$(PREFIX)/share/man

ifndef CC
CC = gcc
endif
LD = $(CC)

EXECS = smp_rep_general smp_rep_manufacturer smp_discover smp_phy_control \
	smp_rep_phy_err_log smp_rep_phy_sata smp_rep_route_info \
	smp_read_gpio smp_conf_route_info smp_write_gpio smp_phy_test \
	smp_discover_list smp_conf_general smp_rep_exp_route_tbl

MAN_PGS = smp_utils.8 smp_rep_general.8 smp_rep_manufacturer.8 \
	  smp_discover.8 smp_phy_control.8 smp_rep_phy_err_log.8 \
	  smp_rep_phy_sata.8 smp_rep_route_info.8 smp_read_gpio.8 \
	  smp_conf_route_info.8 smp_write_gpio.8 smp_phy_test.8 \
	  smp_discover_list.8 smp_conf_general.8 smp_rep_exp_route_tbl.8
MAN_PREF = man8

OS_FLAGS = -DSMP_UTILS_LINUX
LARGE_FILE_FLAGS = -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
EXTRA_FLAGS = $(OS_FLAGS) $(LARGE_FILE_FLAGS)

INCLUDES = -I include

# may be overridden by 'make -e'
CFLAGS = -g -O2

# MY_CFLAGS = -Wall -W $(EXTRA_FLAGS)
MY_CFLAGS = -Wall -W $(EXTRA_FLAGS)
# MY_CFLAGS = -Wall -W -pedantic -std=c99 $(EXTRA_FLAGS)

CFLAGS_PTHREADS = -D_REENTRANT

LDFLAGS = 
# LDFLAGS = -v -lm

# $(SDIRS):
# 	set -e; for i in $(SDIRS); \
# 	do $(MAKE) -C $$i CFLAGS="$(CFLAGS)" ; done

.c.o:
	$(CC) $(INCLUDES) $(CFLAGS) $(MY_CFLAGS) -c -o $@ $<

all: sub_mpt sub_sas_tpl sub_sgv4 libsmp.a $(EXECS)

sub_mpt:
	cd mpt && $(MAKE)

sub_sas_tpl:
	cd sas_tpl && $(MAKE)

sub_sgv4:
	cd sgv4 && $(MAKE)

libsmp.a : sub_mpt sub_sas_tpl sub_sgv4 smp_lib.o smp_interface_sel.o
	ar r libsmp.a smp_lib.o smp_interface_sel.o mpt/smp_mptctl_io.o  \
	sas_tpl/smp_portal_intf.o sgv4/smp_sgv4_io.o

depend dep:
	cd mpt && $(MAKE) dep
	cd sas_tpl && $(MAKE) dep
	cd sgv4 && $(MAKE) dep
	for i in *.c; do $(CC) $(INCLUDES) $(CFLAGS) -M $$i; \
	done > .depend

clean:
	cd mpt && $(MAKE) clean
	cd sas_tpl && $(MAKE) clean
	cd sgv4 && $(MAKE) clean
	/bin/rm -f *.o $(EXECS) core* .depend *.a *.la *.lo
	/bin/rm -rf .libs
	/bin/rm -f build-stamp configure-stamp debian/files debian/smp-utils.substvars
	/bin/rm -rf debian/tmp debian/smp-utils

smp_rep_general: smp_rep_general.o libsmp.a
	$(LD) -o $@ $(LDFLAGS) $^

smp_rep_manufacturer: smp_rep_manufacturer.o libsmp.a
	$(LD) -o $@ $(LDFLAGS) $^

smp_discover: smp_discover.o libsmp.a
	$(LD) -o $@ $(LDFLAGS) $^

smp_phy_control: smp_phy_control.o libsmp.a
	$(LD) -o $@ $(LDFLAGS) $^

smp_rep_phy_err_log: smp_rep_phy_err_log.o libsmp.a
	$(LD) -o $@ $(LDFLAGS) $^

smp_rep_phy_sata: smp_rep_phy_sata.o libsmp.a
	$(LD) -o $@ $(LDFLAGS) $^

smp_rep_route_info: smp_rep_route_info.o libsmp.a
	$(LD) -o $@ $(LDFLAGS) $^

smp_read_gpio: smp_read_gpio.o libsmp.a
	$(LD) -o $@ $(LDFLAGS) $^

smp_conf_route_info: smp_conf_route_info.o libsmp.a
	$(LD) -o $@ $(LDFLAGS) $^

smp_write_gpio: smp_write_gpio.o libsmp.a
	$(LD) -o $@ $(LDFLAGS) $^

smp_phy_test: smp_phy_test.o libsmp.a
	$(LD) -o $@ $(LDFLAGS) $^

smp_discover_list: smp_discover_list.o libsmp.a
	$(LD) -o $@ $(LDFLAGS) $^

smp_conf_general: smp_conf_general.o libsmp.a
	$(LD) -o $@ $(LDFLAGS) $^

smp_rep_exp_route_tbl: smp_rep_exp_route_tbl.o libsmp.a
	$(LD) -o $@ $(LDFLAGS) $^

install: $(EXECS)
	install -d $(INSTDIR)
	for name in $^; \
	 do install -s -o root -g root -m 755 $$name $(INSTDIR); \
	done
	install -d $(MANDIR)/$(MAN_PREF)
	for mp in $(MAN_PGS); \
	 do install -o root -g root -m 644 doc/$$mp $(MANDIR)/$(MAN_PREF); \
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
