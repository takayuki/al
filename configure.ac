#
#  Al -- an implementation of the adaptive locks
#
#  Copyright (C) 2008, University of Oregon
#  All rights reserved.
# 
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are
#  met:
#  
#    * Redistributions of source code must retain the above copyright
#      notice, this list of conditions and the following disclaimer.
#  
#    * Redistributions in binary form must reproduce the above copyright
#      notice, this list of conditions and the following disclaimer in
#      the documentation and/or other materials provided with the
#      distribution.
#  
#    * Neither the name of University of Oregon nor the names of its
#      contributors may be used to endorse or promote products derived
#      from this software without specific prior written permission.
#  
#  THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF OREGON ``AS IS'' AND ANY
#  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
#  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
#  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL STANFORD UNIVERSITY BE LIABLE
#  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
#  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
#  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
#  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
#  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
#  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
#  THE POSSIBILITY OF SUCH DAMAGE.
#
AC_INIT([CIS 609 Final Project],[0.14],[tusui@uoregon.edu],[al])
AC_PREREQ([2.59])
AC_CONFIG_SRCDIR([atomic.ml])
AC_CONFIG_AUX_DIR([build])
AC_CONFIG_HEADER([config.h])
m4_include([build/acx_pthread.m4])
m4_include([build/check_gnu_make.m4])
m4_include([build/ax_cflags_gcc_option.m4])
m4_include([build/ax_tls.m4])
AC_ARG_ENABLE([m32],[AS_HELP_STRING([--enable-m32],[generate 32-bit code])],
              [CFLAGS="-m32 $CFLAGS"; LDFLAGS="$LDFLAGS -m32"])
AC_ARG_ENABLE([m64],[AS_HELP_STRING([--enable-m64],[generate 64-bit code])],
              [CFLAGS="-m64 $CFLAGS"; LDFLAGS="$LDFLAGS -m64"])
AC_ARG_ENABLE([pg],[AS_HELP_STRING([--enable-pg],[generate code to profile])],
              [CFLAGS="-pg $CFLAGS"; LDFLAGS="$LDFLAGS -pg"])
AC_ARG_ENABLE([tls],[AS_HELP_STRING([--disable-tls],
              [disable thread local storage (TLS)])])
AC_PROG_CC([gcc])
AC_PROG_INSTALL
AC_CHECK_PROG([AR],[ar],[ar])
AX_CFLAGS_GCC_OPTION([-O3])
AX_CFLAGS_GCC_OPTION([-Winline])
AX_CFLAGS_GCC_OPTION([-fno-implement-inlines])
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
AS_IF([test "x$enable_tls" = xno], [], [AX_TLS])
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
AC_CONFIG_FILES([stamp/lib/Makefile])
AC_CONFIG_FILES([stamp/bayes/Makefile])
AC_CONFIG_FILES([stamp/genome/Makefile])
AC_CONFIG_FILES([stamp/kmeans/Makefile])
AC_CONFIG_FILES([stamp/labyrinth/Makefile])
AC_CONFIG_FILES([stamp/vacation/Makefile])
AC_CONFIG_FILES([bench/Makefile])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
