
if OS_LINUX

libsmputils1_la_SOURCES = \
	smp_lib.c \
	smp_lin_bsg.c \
	smp_lin_sel.c \
	smp_mptctl_io.c

EXTRA_libsmputils1_la_SOURCES = \
	smp_dummy.c

endif


if OS_FREEBSD

libsmputils1_la_SOURCES = \
	smp_lib.c \
	smp_fre_sel.c

EXTRA_libsmputils1_la_SOURCES = \
	smp_dummy.c

endif


if OS_SOLARIS

libsmputils1_la_SOURCES = \
	smp_lib.c \
	smp_sol_usmp.c

EXTRA_libsmputils1_la_SOURCES = \
	smp_dummy.c

endif



AM_CFLAGS = -I ../include -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -W
# AM_CFLAGS = -I ../include -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -W -pedantic -std=c99

lib_LTLIBRARIES = libsmputils1.la

libsmputils1_la_LDFLAGS = -version-info 1:0:0

libsmputils1_la_LIBADD = @os_libs@

## libsmputils1_la_LIBADD = @GETOPT_O_FILES@ @os_libs@
## libsmputi1s1_la_DEPENDENCIES = @GETOPT_O_FILES@

