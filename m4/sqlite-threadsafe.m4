# sqlite-threadsafe.m4 serial 1

dnl Copyright (C) 2010 Aleksander Morgado <aleksander@gnu.org>
dnl This file is free software; unlimited permission to copy and/or distribute
dnl  it is given, with or without modifications, as long as this notice is
dnl  preserved.

dnl This program will execute the sqlite3_threadsafe() method to check
dnl  whether the sqlite3 library was compiled in threadsafe mode or not,
dnl  and will fill the ax_cv_sqlite_threadsafe cached variable accordingly.
dnl See http://sqlite.org/c3ref/threadsafe.html for more information.

dnl Once this m4 macro has been evaluated, you can for example issue an error
dnl  when sqlite3 was not compiled thread-safe:
dnl
dnl  AX_SQLITE_THREADSAFE
dnl  if test "x$ax_cv_sqlite_threadsafe" != "xyes"; then
dnl    AC_MSG_ERROR([sqlite3 is not compiled in a thread-safe mode])
dnl  fi


AC_DEFUN([AX_SQLITE_THREADSAFE],
[
  AC_REQUIRE([AC_PROG_CC])

  AC_CHECK_HEADERS([sqlite3.h])
  AC_CHECK_LIB([sqlite3],[sqlite3_threadsafe])

  AC_CACHE_CHECK([whether sqlite was compiled thread-safe],
                 [ax_cv_sqlite_threadsafe],
  [
  AC_RUN_IFELSE([AC_LANG_SOURCE([[
#include <sqlite3.h>
int main ()
{
  /* sqlite3_threadsafe() returns the value of the SQLITE_THREADSAFE
   * preprocessor macro used when compiling. If this is 0, sqlite3
   * library was NOT compiled in a thread-safe mode */

  return sqlite3_threadsafe () == 0 ? -1 : 0;
}

  ]])],
       [ax_cv_sqlite_threadsafe=yes],
       [ax_cv_sqlite_threadsafe=no],
       [ax_cv_sqlite_threadsafe=no])])
])

