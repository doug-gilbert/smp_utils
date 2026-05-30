// This file (config.h.in.cmake) is used as a template to generate the
// config.h file in the first step of a cmake build. This file should be
// kept under source control (e.g. svn or git) while config.h can be
// deleted after a suucessful build. After a failed build config.h
// should be examined.

#cmakedefine OS_LINUX  1
#cmakedefine SMP_LIB_LINUX 1 
#cmakedefine SMP_LIB_ANDROID 1 
#cmakedefine OS_ANDROID  1
#cmakedefine OS_FREEBSD  1
#cmakedefine SMP_LIB_FREEBSD  1
#cmakedefine OS_NetBSD  1
#cmakedefine SMP_LIB_NETBSD  1
#cmakedefine OS_OPENBSD  1
#cmakedefine SMP_LIB_OPENBSD 1 
#cmakedefine OS_SOLARIS 1 
#cmakedefine SMP_LIB_SOLARIS 1
#cmakedefine OS_AIX 1
#cmakedefine SMP_LIB_AIX 1
#cmakedefine OS_HAIKU 1
#cmakedefine SMP_LIB_HAIKU 1
#cmakedefine OS_WIN32 1 
#cmakedefine SMP_LIB_WIN32  1
#cmakedefine SMP_LIB_MINGW  1
#cmakedefine SMP_LIB_CYGWIN  1

#cmakedefine HAVE_POSIX_MEMALIGN 1
#cmakedefine HAVE_BYTESWAP_H 1
#cmakedefine HAVE_GETOPT_H 1
#cmakedefine HAVE_GETOPT_LONG 1
#cmakedefine HAVE_SYSCONF 1
#cmakedefine HAVE_LINUX_TYPES_H 1
#cmakedefine HAVE_LINUX_BSG_H 1

#cmakedefine IGNORE_FAST_LEBE 1
#cmakedefine IGNORE_LINUX_BSG 1

#cmakedefine NEED_GETOPT_H 1
#cmakedefine NEED_GETOPT_LONG 1

#define BUILD_TIME "@BUILD_TIME@"

// #define SMP_LIB_LINUX @OS_LINUX@
// #define SMP_LIB_FREEBSD @OS_FREEBSD@
// #cmakedefine01 SMP_LIB_AIX @SMP_LIB_AIX@

// # This will generate a line in the output_file. Then in CMLists.txt:
// #      set(FEATURE_COMMENT "//")
// @FEATURE_COMMENT@#define OPTIONAL_SETTING 1

