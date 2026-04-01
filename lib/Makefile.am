
if OS_LINUX

libsmputils1_la_SOURCES = \
	smp_lib.c \
	smp_lin_bsg.c \
	smp_lin_sel.c \
	smp_mptctl_io.c \
	smp_aac_io.c

endif


if OS_FREEBSD

libsmputils1_la_SOURCES = \
	smp_lib.c \
	smp_fre_cam.c

endif


if OS_SOLARIS

libsmputils1_la_SOURCES = \
	smp_lib.c \
	smp_sol_usmp.c

endif

EXTRA_libsmputils1_la_SOURCES = \
	smp_dummy.c

# libsmputils1_ladir = .

EXTRA_DIST = \
	smp_lin_bsg.h \
	aacraid.h \
	mpi.h \
	mpi_sas.h \
	mpi_type.h \
	mptctl.h \
	smp_aac_io.h \
	smp_mptctl_glue.h \
	smp_mptctl_io.h

# for testing with various compilers
## CC = gcc
## CC = g++
## CC = clang
## CC = clang++

# -std=<s> can be c99, c11, gnu11, etc. Default is gnu89 (gnu90 is the same)
AM_CPPFLAGS = -iquote ${top_srcdir}/include -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64
AM_CFLAGS = -Wall -W
## AM_CFLAGS = -Wall -W -pedantic -std=c11
## AM_CFLAGS = -Wall -W -pedantic -std=c++14
## AM_CFLAGS = -Wall -W -pedantic -std=c++20

lib_LTLIBRARIES = libsmputils1.la

libsmputils1_la_LDFLAGS = -version-info 1:0:0

## libsmputils1_la_LIBADD =

distclean-local:
	rm -rf \
	  .deps \
	  Makefile.in

