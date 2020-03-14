AC_INIT(smp_utils, 1.00, dgilbert@interlog.com)

AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AM_MAINTAINER_MODE
AM_CONFIG_HEADER(config.h)

AC_PROG_CC
# AC_PROG_CXX
AC_PROG_INSTALL

# AM_PROG_AR is supported and needed since automake v1.12+
ifdef([AM_PROG_AR], [AM_PROG_AR], [])

# Adding libtools to the build seems to bring in C++ environment
AC_PROG_LIBTOOL

# check for headers
AC_HEADER_STDC

# check for functions
AC_CHECK_FUNCS(posix_memalign)

AC_CANONICAL_HOST

AC_DEFINE_UNQUOTED(SMP_UTILS_BUILD_HOST, "${host}", [smp_utils Build Host])

check_for_linux_nvme_headers() {
        AC_CHECK_HEADERS([linux/types.h linux/bsg.h linux/kdev_t.h], [], [],
                     [[#ifdef HAVE_LINUX_TYPES_H
                     # include <linux/types.h>
                     #endif
                     ]])
}

case "${host}" in
        *-*-android*)
                AC_DEFINE_UNQUOTED(SMP_LIB_ANDROID, 1, [smp_utils on android])
                AC_DEFINE_UNQUOTED(SMP_LIB_LINUX, 1, [smp_utils on linux])
                check_for_linux_nvme_headers;;
        *-*-freebsd*|*-*-kfreebsd*-gnu*)
                AC_DEFINE_UNQUOTED(SMP_LIB_FREEBSD, 1, [smp_utils on FreeBSD])
                LIBS="$LIBS -lcam";;
        *-*-solaris*)
		AC_DEFINE_UNQUOTED(SMP_UTILS_SOLARIS, 1, [smp_utils on Solaris]);;
        *-*-linux-gnu* | *-*-linux* | *)
                AC_DEFINE_UNQUOTED(SMP_LIB_LINUX, 1, [smp_utils on linux])
                check_for_linux_nvme_headers;;
esac

# Define platform-specific symbol.
AM_CONDITIONAL(OS_FREEBSD, [echo $host_os | grep 'freebsd' > /dev/null])
AM_CONDITIONAL(OS_LINUX, [echo $host_os | grep '^linux' > /dev/null])
AM_CONDITIONAL(OS_SOLARIS, [echo $host_os | grep '^solaris' > /dev/null])
AM_CONDITIONAL(OS_ANDROID, [echo $host_os | grep 'android' > /dev/null])

AC_CHECK_LIB(smputils1, smp_send_req,
  [SMPUTILS_LIBS="-lsmputils1"; have_smputils=yes], have_smputils=no)
AC_SUBST(SMPUTILS_LIBS)
AM_CONDITIONAL(HAVE_SMPUTILS, test x"$have_smputils" = xyes)

AC_OUTPUT(Makefile include/Makefile lib/Makefile src/Makefile doc/Makefile)
