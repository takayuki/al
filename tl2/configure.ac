AC_INIT([TL2-x86],[0.9.3],[tusui@uoregon.edu],[tl2-x86])
AC_PREREQ([2.59])
AC_CONFIG_SRCDIR([tl2.h])
AC_CONFIG_HEADER([config.h])
m4_include([build/ax_cflags_gcc_option.m4])
AC_ARG_ENABLE([m32],[AS_HELP_STRING([--enable-m32],[generate 32-bit code])],
              [CFLAGS="-m32 $CFLAGS"; LDFLAGS="$LDFLAGS -m32"])
AC_ARG_ENABLE([m64],[AS_HELP_STRING([--enable-m64],[generate 64-bit code])],
              [CFLAGS="-m64 $CFLAGS"; LDFLAGS="$LDFLAGS -m64"])
AC_PROG_CC([gcc])
AX_CFLAGS_GCC_OPTION([-Wall])
AX_CFLAGS_GCC_OPTION([-Winline])
AX_CFLAGS_GCC_OPTION([-O3])
AX_CFLAGS_GCC_OPTION([-mcpu=v9])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
