AC_INIT(smp_utils, 0.97, dgilbert@interlog.com)

AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE
AM_CONFIG_HEADER(config.h)

AC_PROG_CC
# AC_PROG_CXX
AC_PROG_INSTALL

# Adding libtools to the build seems to bring in C++ environment
AC_PROG_LIBTOOL

# check for headers
AC_HEADER_STDC
AC_CHECK_HEADERS([linux/types.h linux/bsg.h], [], [],
     [[#ifdef HAVE_LINUX_TYPES_H
     # include <linux/types.h>
     #endif
     ]])

# AC_CHECK_FUNCS(getopt_long,
# 	       GETOPT_O_FILES='',
# 	       GETOPT_O_FILES='getopt_long.o')
# AC_SUBST(GETOPT_O_FILES)

AC_CANONICAL_HOST

AC_DEFINE_UNQUOTED(SMP_UTILS_BUILD_HOST, "${host}", [smp_utils Build Host])

case "${host}" in
        *-*-linux-gnu*)
                AC_SUBST([os_deps], ['sg_pt_linux.o'])
		AC_DEFINE_UNQUOTED(SMP_UTILS_LINUX, 1, [smp_utils on linux])
                AC_SUBST([os_libs], ['']) ;;
        *-*-linux*)
                AC_SUBST([os_deps], ['sg_pt_linux.o'])
		AC_DEFINE_UNQUOTED(SMP_UTILS_LINUX, 1, [smp_utils on linux])
                AC_SUBST([os_libs], ['']) ;;
        *-*-freebsd*|*-*-kfreebsd*-gnu*)
                AC_SUBST([os_deps], ['sg_pt_freebsd.o'])
		AC_DEFINE_UNQUOTED(SMP_UTILS_FREEBSD, 1, [smp_utils on FreeBSD])
                AC_SUBST([os_libs], ['-lcam']);;
        *-*-solaris*)
                AC_SUBST([os_deps], ['sg_pt_solaris.o'])
		AC_DEFINE_UNQUOTED(SMP_UTILS_SOLARIS, 1, [smp_utils on Solaris])
                AC_SUBST([os_libs], ['']);;
        *)
                AC_SUBST([os_deps], ['sg_pt_linux.o'])
		AC_DEFINE_UNQUOTED(SMP_UTILS_LINUX, 1, [assume smp_utils on linux])
                AC_SUBST([os_libs], ['']) ;;
esac

# Define platform-specific symbol.
AM_CONDITIONAL(OS_FREEBSD, [echo $host_os | grep 'freebsd' > /dev/null])
AM_CONDITIONAL(OS_LINUX, [echo $host_os | grep '^linux' > /dev/null])
AM_CONDITIONAL(OS_SOLARIS, [echo $host_os | grep '^solaris' > /dev/null])

AC_CHECK_LIB(smputils1, smp_send_req,
  [SMPUTILS_LIBS="-lsmputils1"; have_smputils=yes], have_smputils=no)
AC_SUBST(SMPUTILS_LIBS)
AM_CONDITIONAL(HAVE_SMPUTILS, test x"$have_smputils" = xyes)

AC_OUTPUT(Makefile include/Makefile lib/Makefile src/Makefile doc/Makefile)
