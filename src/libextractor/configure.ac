# Process this file with autoconf to produce a configure script.
AC_PREREQ(2.57)
AC_INIT([libextractor], [0.5.8], [bug-libextractor@gnu.org])
AC_REVISION($Revision$)
AM_INIT_AUTOMAKE([libextractor], [0.5.8])
AM_CONFIG_HEADER(src/include/config.h)

AH_TOP([#define _GNU_SOURCE  1])

# Checks for programs.
AC_CANONICAL_HOST

# The problem that I'm trying to solve here by specifying gcc-3.3 and gcc2
# in the search list ahead of the other stuff is that gcc-3.1 on Mac OS X
# does not work for some of the pdf extractor code.  Now, it would seem
# that we could have a conditional on $host_os that puts just the right
# amount of crazyness into the checks only on the Mac.  Unfortunately, this
# will not work, because (according to some mailing lists and painful
# personal experiences) AC_PROG_CXX does not work inside of conditionals.
# So, instead, we do this crazyness globally.  The list of compilers that
# you see below is taken right out of the autoconf (version 2.57) sources,
# so it should not impede our portability.
AC_PROG_CC(gcc-4.0 gcc-3.3 gcc2 gcc cc /usr/ucb/cc cl)
AC_PROG_CPP
AC_PROG_CXX(g++-4.0 g++-3.3 g++2 g++ c++ gpp aCC CC cxx cc++ cl FCC KCC RCC xlC_r xlC)

AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_DISABLE_STATIC

case "$host_os" in
linux*)
     AC_DEFINE(LINUX,1,[This is a Linux system])
     AC_DEFINE_UNQUOTED(LINUX,1,[This is a LINUX system])
     AM_CONDITIONAL(HAVE_GNU_LD, true)    
     XTRA_CPPLIBS=-lstdc++
     ;;
freebsd*)
     AC_DEFINE_UNQUOTED(SOMEBSD,1,[This is a BSD system])
     AC_CHECK_LIB(c_r, pthread_create)
     AM_CONDITIONAL(HAVE_GNU_LD, true)
     ;;
openbsd*)
     AC_DEFINE_UNQUOTED(SOMEBSD,1,[This is a BSD system])
     AC_CHECK_LIB(c_r, pthread_create)
     AM_CONDITIONAL(HAVE_GNU_LD, true)
     ;;
netbsd*)
     AC_DEFINE_UNQUOTED(SOMEBSD,1,[This is a BSD system])
     AC_CHECK_LIB(c_r, pthread_create)
     AM_CONDITIONAL(HAVE_GNU_LD, true)
     ;;
*solaris*)
     AC_DEFINE_UNQUOTED(SOLARIS,1,[This is a Solaris system])
     AC_CHECK_LIB(resolv, res_init)
     XTRA_CPPLIBS=-lstdc++
     AM_CONDITIONAL(HAVE_GNU_LD, false)
     ;;
darwin*)
     AC_DEFINE_UNQUOTED(DARWIN,1,[This is a Darwin system])
     AM_CONDITIONAL(HAVE_GNU_LD, false)
     ;;
cygwin*)
     AC_DEFINE_UNQUOTED(CYGWIN,1,[This is a CYGWIN system])
     LDFLAGS="$LDFLAGS -no-undefined"
     AM_CONDITIONAL(HAVE_GNU_LD, false)
     ;;
mingw*)
     AC_DEFINE_UNQUOTED(MINGW,1,[This is a MinGW system])
     AC_DEFINE_UNQUOTED(WINDOWS,1,[This is a Windows system])
     AC_CHECK_LIB(intl, gettext)
     AC_CHECK_LIB(ws2_32, _head_libws2_32_a,,[AC_MSG_ERROR([libextractor requires Winsock2])])
     AC_CHECK_LIB(plibc, plibc_init,,[AC_MSG_ERROR([libextractor requires PlibC])])
     LDFLAGS="$LDFLAGS -no-undefined -Wl,--export-all-symbols"
     AM_CONDITIONAL(HAVE_GNU_LD, true)
     ;;
*)
     AC_MSG_RESULT(Unrecognised OS $host_os)
     AC_DEFINE_UNQUOTED(OTHEROS,1,[Some strange OS])
     AC_MSG_RESULT(otheros)
     AM_CONDITIONAL(HAVE_GNU_LD, false)
     ;;
esac

AC_LIBTOOL_DLOPEN
AC_LIBLTDL_INSTALLABLE
AC_SUBST(LTDLINCL)
AC_SUBST(LIBLTDL)
AM_PROG_LIBTOOL
AC_LIB_LTDL
AC_CONFIG_SUBDIRS(libltdl)

AM_ICONV

# We define the paths here, because MinGW/GCC expands paths
# passed through the command line ("-DLOCALEDIR=..."). This would
# lead to hard-coded paths ("C:\mingw\mingw\bin...") that do
# not contain the actual installation.
AC_DEFINE_DIR([LOCALEDIR], [datadir/locale], [gettext catalogs])


# large file support
AC_SYS_LARGEFILE
AC_FUNC_FSEEKO

AM_CONDITIONAL(CYGWIN, test "$build_os" = "cygwin")
AM_CONDITIONAL(MINGW, test "$build_os" = "mingw32")

# use '-fno-strict-aliasing', but only if the compiler can take it
if gcc -fno-strict-aliasing -S -o /dev/null -xc /dev/null >/dev/null 2>&1; 
then
 CFLAGS="-fno-strict-aliasing $CFLAGS"
fi

# Checks for libraries.

# the library tests that follow should not result in every plugin linking to them,
# thus we need to backup LIBS and restore
LIBSOLD=$LIBS

# Redhat 9, gcc 3.2, libextractor_pdf barfs if we don't check for this
AC_CHECK_LIB(stdc++, fclose)

AC_CHECK_HEADERS(langinfo.h)
AC_CHECK_LIB(dl, dlopen)
LIBS=$LIBSOLD
# FIXME: allow --with-oggvorbis=PFX

# test if we have vorbisfile
# prior versions had ov_open_callbacks in libvorbis, test that, too.
AC_CHECK_LIB(vorbisfile, ov_open_callbacks,
        [AM_CONDITIONAL(HAVE_VORBISFILE, true)
         AC_DEFINE(HAVE_VORBISFILE,1,[Have vorbisfile])],
        [AM_CONDITIONAL(HAVE_VORBISFILE, false)])
AC_CHECK_LIB(vorbisfile, vorbis_comment_query,
        [AM_CONDITIONAL(NEED_VORBIS, false)],
        [AM_CONDITIONAL(NEED_VORBIS, true)])

AC_CHECK_LIB(z, inflate,
        [AM_CONDITIONAL(HAVE_ZLIB, true)
        AC_DEFINE(HAVE_ZLIB,1,[Have zlib])],
        [AM_CONDITIONAL(HAVE_ZLIB, false)])

AC_CHECK_LIB(bz2, BZ2_decompress,
        [AM_CONDITIONAL(HAVE_BZ2, true)
        AC_DEFINE(HAVE_LIBBZ2,1,[Have libbz2])],
        [AM_CONDITIONAL(HAVE_BZ2, false)])

# restore LIBS
LIBS=$LIBSOLD

# Special check for broken Operating Systems (OS X)
AC_CACHE_CHECK(whether ${CC-cc} accepts -no-cpp-precomp,
cv_prog_cc_darwin_cpp_precomp,
[echo 'void f(){}' > conftest.c
if test -z "`${CC-cc} -no-cpp-precomp -c conftest.c 2>&1`"; then
  cv_prog_cc_darwin_cpp_precomp=yes
else
  cv_prog_cc_darwin_cpp_precomp=no
fi
rm -f conftest*
])
if test $cv_prog_cc_darwin_cpp_precomp = yes; then
  CFLAGS="$CFLAGS -no-cpp-precomp"
fi


# Checks for header files.
AC_HEADER_STDC
AC_HEADER_DIRENT
AC_HEADER_STDBOOL
AC_CHECK_HEADERS([fcntl.h netinet/in.h stdlib.h string.h unistd.h libintl.h limits.h stddef.h zlib.h])
AC_CHECK_HEADERS([ltdl.h iconv.h bzlib.h])
# fixme, we need to die here if a header is not found!

AC_CHECK_HEADERS([vorbis/vorbisfile.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_PID_T
GNUPG_CHECK_ENDIAN

# Checks for library functions.
AC_FUNC_MEMCMP
AC_FUNC_VPRINTF
AC_FUNC_MMAP
AC_FUNC_STAT
AC_FUNC_ERROR_AT_LINE
AC_CHECK_FUNCS([mkstemp strndup munmap strcasecmp strdup strncasecmp memmove memset strtoul floor getcwd pow setenv sqrt strchr strcspn strrchr strnlen strndup ftruncate])

AM_GNU_GETTEXT_VERSION(0.13)
AM_GNU_GETTEXT([external])

# check for GNU LD
AC_LIB_PROG_LD_GNU

# check for glib >= 2.0.0
AC_MSG_CHECKING(for glib)
AM_PATH_GLIB_2_0(2.0.0,
	         without_glib=false,
                 without_glib=true)
AC_MSG_CHECKING([whether glib is disabled])
AC_ARG_ENABLE(glib,
  [AC_HELP_STRING([--disable-glib],[disable glib support])],
  [case "$enableval" in
   no)  AC_MSG_RESULT(disabled)
        without_glib=true
        ;;
   *)   AC_MSG_RESULT(allowed)
        ;;
   esac])

if test x$without_glib != xtrue
then
  if test $with_gnu_ld == yes
  then
# We need both GNU LD and GLIB here!
    AM_CONDITIONAL(HAVE_GLIB,true)
    AC_DEFINE(HAVE_GLIB, 1, [Have glib])
  else
# We may have GLIB, but without GNU LD we must not use it!
    AM_CONDITIONAL(HAVE_GLIB,false)
    AC_DEFINE(HAVE_GLIB, 0, [Have glib, but not GNU LD])
  fi
else
 AM_CONDITIONAL(HAVE_GLIB,false)
 AC_DEFINE(HAVE_GLIB, 0, [Have glib])
fi
        

# check for gtk >= 2.6.0
AC_MSG_CHECKING(for gtk)
AM_PATH_GTK_2_0(2.6.0,without_gtk=false,without_gtk=true)
AM_CONDITIONAL(HAVE_GTK, test x$without_gtk != xtrue)
if test $without_gtk != true
then
 AC_DEFINE_UNQUOTED([HAVE_GTK], 1, [We have GTK])
else
 AM_CONDITIONAL(HAVE_GTK,false)
fi



printable=0
AC_MSG_CHECKING([whether to enable printable extractors])
AC_ARG_ENABLE(printable,
 [AC_HELP_STRING([--enable-printable],[Enable printable support])
  AC_HELP_STRING([--disable-printable],[Disable printable support])],
 [case "$enableval" in
  no)	AC_MSG_RESULT(no)
        printable=0
	;;
  *)    AC_MSG_RESULT(yes)
        printable=1
	;;
  esac],
  [     AC_MSG_RESULT(yes)
        printable=1])
AM_CONDITIONAL(HAVE_PRINTABLE, test x$printable != x0)

exiv2=1
AC_MSG_CHECKING([whether to enable exiv2 extractor])
AC_ARG_ENABLE(exiv2,
 [AC_HELP_STRING([--enable-exiv2],[Enable exiv2 support])
  AC_HELP_STRING([--disable-exiv2],[Disable exiv2 support])],
 [case "$enableval" in
  no)	AC_MSG_RESULT(no)
        exiv2=0
	;;
  *)    AC_MSG_RESULT(yes)
        exiv2=1
	;;
  esac],
  [     AC_MSG_RESULT(yes)
        exiv2=1])
AM_CONDITIONAL(HAVE_EXIV2, test x$exiv2 != x0)
AC_DEFINE_UNQUOTED([HAVE_EXIV2], $exiv2, [We use EXIV2])


AC_SUBST(CPPFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(XTRA_CPPLIBS)

AC_CONFIG_FILES([Makefile po/Makefile.in m4/Makefile 
doc/Makefile
src/Makefile
src/include/Makefile
src/main/Makefile
src/plugins/Makefile
src/plugins/ole2/Makefile
src/plugins/oo/Makefile
src/plugins/pdf/Makefile
src/plugins/rpm/Makefile
src/plugins/hash/Makefile
src/plugins/thumbnail/Makefile
src/plugins/exiv2/Makefile
src/test/Makefile
])

AC_OUTPUT
