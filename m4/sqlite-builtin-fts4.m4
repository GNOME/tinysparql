AC_DEFUN([AX_SQLITE_BUILTIN_FTS4],
[
  AC_REQUIRE([AC_PROG_CC])

  OLD_CFLAGS="$CFLAGS"
  OLD_LIBS="$LIBS"
  CFLAGS="$SQLITE3_CFLAGS"
  LIBS="$SQLITE3_LIBS"

  AC_CHECK_HEADERS([sqlite3.h])

  AC_CACHE_CHECK([whether SQLite3 has required FTS features],
                 [ax_cv_sqlite_builtin_fts4],
  [
  AC_RUN_IFELSE(
    [AC_LANG_PROGRAM([[#include <sqlite3.h>]],
                     [[sqlite3 *db;
                       int rc;
                       rc = sqlite3_open(":memory:", &db);
                       if (rc!=SQLITE_OK) return -1;
                       rc = sqlite3_exec(db, "create table a(text)", 0, 0, 0);
                       if (rc!=SQLITE_OK) return -1;
                       rc = sqlite3_exec(db, "create virtual table t using fts4(content='a',text)", 0, 0, 0);
                       if (rc!=SQLITE_OK) return -1;]])],
    [ax_cv_sqlite_builtin_fts4=yes],
    [ax_cv_sqlite_builtin_fts4=no],
    [ax_cv_sqlite_builtin_fts4=no])])

  CFLAGS="$OLD_CFLAGS"
  LIBS="$OLD_LIBS"
])

