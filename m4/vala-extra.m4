dnl Copyright (C) 2011 Nokia <ivan.frade@nokia.com>
dnl This file is free software; unlimited permission to copy and/or distribute
dnl  it is given, with or without modifications, as long as this notice is
dnl  preserved.

dnl AX_VALA_CHECK_VERSION([MIN_VERSION], [ACTION-IF-TRUE], [ACTION-IF-FALSE])
dnl ---------------------------------------------------------------------------
dnl Run ACTION-IF-TRUE if the vala compiler is installed AND has version >= VERSION.
dnl Run ACTION-IF-FALSE otherwise.

dnl This macro doesn't check if valac is installed (use AM_PROG_VALAC or your
dnl own AC_PATH_PROG to check that)

AC_DEFUN([AX_VALA_CHECK_VERSION],
[
  AC_MSG_CHECKING([valac generates proper GIR])
  AS_IF([test "x$VALAC" != "x"], [], [AC_PATH_PROG([VALAC], [valac], [])])
  am__vala_version=`$VALAC --version | sed 's/Vala  *//'`
  AS_VERSION_COMPARE([$1], ["$am__vala_version"], 
       [AC_MSG_RESULT([yes]) 
        $2], 
       [AC_MSG_RESULT([yes]) 
        $2], 
       [AC_MSG_RESULT([no]) 
        $3])
])
