AC_INIT([CIS 609 Final Project],[0.9],[tusui@uoregon.edu],[al])
AC_PREREQ([2.59])
AC_CONFIG_SRCDIR([atomic.ml])
AC_CONFIG_AUX_DIR([build])
AC_CONFIG_HEADER([config.h])
m4_include([build/acx_pthread.m4])
m4_include([build/check_gnu_make.m4])
m4_include([build/ax_cflags_gcc_option.m4])
AC_ARG_ENABLE([m32],[AS_HELP_STRING([--enable-m32],[generate 32-bit code])],
              [CFLAGS="-m32 $CFLAGS"; LDFLAGS="$LDFLAGS -m32"])
AC_ARG_ENABLE([m64],[AS_HELP_STRING([--enable-m64],[generate 64-bit code])],
              [CFLAGS="-m64 $CFLAGS"; LDFLAGS="$LDFLAGS -m64"])
AC_PROG_CC([gcc])
AC_PROG_INSTALL
dnl AX_CFLAGS_GCC_OPTION([-Wall])
AX_CFLAGS_GCC_OPTION([-mcpu=v9])
AC_CHECK_HEADERS([inttypes.h])
AC_CHECK_HEADERS([sched.h])
AC_CHECK_SIZEOF([intptr_t])
AC_SEARCH_LIBS([sched_yield],[rt])
AC_CHECK_FUNCS([gethrtime clock_gettime nrand48])
ACX_PTHREAD
_ac_save_CFLAGS=$CFLAGS
_ac_save_LDFLAGS=$LDFLAGS
_ac_save_LIBS=$LIBS
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
LDFLAGS="$LDFLAGS $PTHREAD_CFLAGS"
LIBS="$PTHREAD_LIBS $LIBS"
AC_CHECK_FUNC([sched_yield],[AC_DEFINE([HAVE_SCHED_YIELD],[1],
              [Define to 1 if you have the POSIX sched_yield function.])])
AC_CHECK_FUNC([pthread_yield],[AC_DEFINE([HAVE_PTHREAD_YIELD],[1],
              [Define to 1 if you have the POSIX pthread_yield function.])])
CFLAGS=$_ac_save_CFLAGS
LDFLAGS=$_ac_save_LDFLAGS
LIBS=$_ac_save_LIBS
AC_CHECK_PROG([OCAMLC],[ocamlc],[ocamlc])
AC_CHECK_PROG([OCAMLOPT],[ocamlopt],[ocamlopt])
AC_CHECK_PROG([OCAMLDEP],[ocamldep],[ocamldep])
CHECK_GNU_MAKE
if test "x$_cv_gnu_make_command" != "x"; then
  AC_SUBST([GNUMAKE],[$_cv_gnu_make_command])
fi
AC_CANONICAL_TARGET
case "$target" in
  *86*linux*) ac_archos=x86_LINUX;;
  *86*openbsd*) ac_archos=x86_OPENBSD;;
  *86*netbsd*) ac_archos=x86_NETBSD;;
  *i386*freebsd*|*amd64*freebsd*)
    if test "x${ARCH}" = "x"; then
       ARCH=`uname -p`
    fi
    ac_archos=${ARCH}_FREEBSD
    ;;
  *86*darwin*) ac_archos=x86_DARWIN;;
  *powerpc*darwin*) ac_archos=ppc_DARWIN;;
  *86*cygwin*) ac_archos=x86_WIN32;;
  *sparc*solaris*) ac_archos=sparc_SOLARIS;;
  *86*solaris*) ac_archos=x86_SOLARIS;;
esac
if test "x$ac_archos" != "x"; then
  AC_SUBST([ARCHOS],[$ac_archos])
fi
AC_CONFIG_SUBDIRS([tl2])
ac_save_configure_args=$ac_configure_args
ac_configure_args="$ac_configure_args EXTRASRCDIRS='..' EXTRAFEATURES='atomic'"
AC_CONFIG_SUBDIRS([cil])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
