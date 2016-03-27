AC_DEFUN([AX_SQLITE_AUTO_EXTENSION],
[
  AC_REQUIRE([AC_PROG_CC])

  OLD_CFLAGS="$CFLAGS"
  OLD_LDFLAGS="$LDFLAGS"
  OLD_LIBS="$LIBS"
  CFLAGS="$SQLITE3_CFLAGS"
  LDFLAGS="$SQLITE3_LDFLAGS"
  LIBS="$SQLITE3_LIBS"

  AC_CHECK_HEADERS([sqlite3.h])

  AC_CACHE_CHECK([whether SQLite3 has extension loading enabled],
                 [ax_cv_sqlite_auto_extension],
  [
  AC_RUN_IFELSE(
    [AC_LANG_PROGRAM([[#include <sqlite3.h>
                       static int initialized = 0;
                       int extEntryPoint(sqlite3 *db, const char **err, void **api){
                         initialized = 1;
                         if (api != 0 && *api != 0)
                           return SQLITE_OK;
                         return SQLITE_ERROR;
                       }]],
                     [[sqlite3 *db;
                       int rc;
                       sqlite3_auto_extension((void (*)(void))extEntryPoint);
                       rc = sqlite3_open(":memory:", &db);
                       if (rc!=SQLITE_OK) return -1;
                       if (initialized==0) return -1]])],
    [ax_cv_sqlite_auto_extension=yes],
    [ax_cv_sqlite_auto_extension=no],
    [ax_cv_sqlite_auto_extension=no])])

  CFLAGS="$OLD_CFLAGS"
  LDFLAGS="$OLD_LDFLAGS"
  LIBS="$OLD_LIBS"
])

