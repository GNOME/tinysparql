#include "config.h"

#define SUPPRESS_WARNINGS 1

/* Define to 1 if you have the <unistd.h> header file. */
#define EXV_HAVE_UNISTD_H HAVE_UNISTD_H

#define EXV_HAVE_STDINT_H HAVE_STDINT_H

/* Define to the address where bug reports for this package should be
   sent. */
#define EXV_PACKAGE_BUGREPORT PACKAGE_BUGREPORT

/* Define to the full name of this package. */
#define EXV_PACKAGE_NAME PACKAGE_NAME

/* Define to the full name and version of this package. */
#define EXV_PACKAGE_STRING PACKAGE_STRING

/* Define to the version of this package. */
#define EXV_PACKAGE_VERSION PACKAGE_VERSION

/* File path seperator */
#define EXV_SEPERATOR_STR DIR_SEPARATOR_STR
#define EXV_SEPERATOR_CHR DIR_SEPARATOR

#if defined __CYGWIN32__ && !defined __CYGWIN__
   /* For backwards compatibility with Cygwin b19 and
      earlier, we define __CYGWIN__ here, so that
      we can rely on checking just for that macro. */
#define __CYGWIN__  __CYGWIN32__
#endif
