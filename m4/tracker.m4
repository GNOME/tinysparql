dnl #########################################################################
AC_DEFUN([AX_DOTS_TO_UNDERSCORES], [
  $1[]_UNDERSCORES=`echo "$$1" | sed -e 's/\./_/g'`
  AC_SUBST($1[]_UNDERSCORES)
]) dnl AX_DOTS_TO_UNDERSCORES

dnl #########################################################################
AC_DEFUN([AX_COMPARE_VERSION], [
  # Used to indicate true or false condition
  ax_compare_version=false

  # Convert the two version strings to be compared into a format that
  # allows a simple string comparison.  The end result is that a version
  # string of the form 1.12.5-r617 will be converted to the form
  # 0001001200050617.  In other words, each number is zero padded to four
  # digits, and non digits are removed.
  AS_VAR_PUSHDEF([A],[ax_compare_version_A])
  A=`echo "$1" | sed -e 's/\([[0-9]]*\)/Z\1Z/g' \
                     -e 's/Z\([[0-9]]\)Z/Z0\1Z/g' \
                     -e 's/Z\([[0-9]][[0-9]]\)Z/Z0\1Z/g' \
                     -e 's/Z\([[0-9]][[0-9]][[0-9]]\)Z/Z0\1Z/g' \
                     -e 's/[[^0-9]]//g'`

  AS_VAR_PUSHDEF([B],[ax_compare_version_B])
  B=`echo "$3" | sed -e 's/\([[0-9]]*\)/Z\1Z/g' \
                     -e 's/Z\([[0-9]]\)Z/Z0\1Z/g' \
                     -e 's/Z\([[0-9]][[0-9]]\)Z/Z0\1Z/g' \
                     -e 's/Z\([[0-9]][[0-9]][[0-9]]\)Z/Z0\1Z/g' \
                     -e 's/[[^0-9]]//g'`

  dnl # In the case of le, ge, lt, and gt, the strings are sorted as necessary
  dnl # then the first line is used to determine if the condition is true.
  dnl # The sed right after the echo is to remove any indented white space.
  m4_case(m4_tolower($2),
  [lt],[
    ax_compare_version=`echo "x$A
x$B" | sed 's/^ *//' | sort -r | sed "s/x${A}/false/;s/x${B}/true/;1q"`
  ],
  [gt],[
    ax_compare_version=`echo "x$A
x$B" | sed 's/^ *//' | sort | sed "s/x${A}/false/;s/x${B}/true/;1q"`
  ],
  [le],[
    ax_compare_version=`echo "x$A
x$B" | sed 's/^ *//' | sort | sed "s/x${A}/true/;s/x${B}/false/;1q"`
  ],
  [ge],[
    ax_compare_version=`echo "x$A
x$B" | sed 's/^ *//' | sort -r | sed "s/x${A}/true/;s/x${B}/false/;1q"`
  ],[
    dnl Split the operator from the subversion count if present.
    m4_bmatch(m4_substr($2,2),
    [0],[
      # A count of zero means use the length of the shorter version.
      # Determine the number of characters in A and B.
      ax_compare_version_len_A=`echo "$A" | awk '{print(length)}'`
      ax_compare_version_len_B=`echo "$B" | awk '{print(length)}'`

      # Set A to no more than B's length and B to no more than A's length.
      A=`echo "$A" | sed "s/\(.\{$ax_compare_version_len_B\}\).*/\1/"`
      B=`echo "$B" | sed "s/\(.\{$ax_compare_version_len_A\}\).*/\1/"`
    ],
    [[0-9]+],[
      # A count greater than zero means use only that many subversions
      A=`echo "$A" | sed "s/\(\([[0-9]]\{4\}\)\{m4_substr($2,2)\}\).*/\1/"`
      B=`echo "$B" | sed "s/\(\([[0-9]]\{4\}\)\{m4_substr($2,2)\}\).*/\1/"`
    ],
    [.+],[
      AC_WARNING(
        [illegal OP numeric parameter: $2])
    ],[])

    # Pad zeros at end of numbers to make same length.
    ax_compare_version_tmp_A="$A`echo $B | sed 's/./0/g'`"
    B="$B`echo $A | sed 's/./0/g'`"
    A="$ax_compare_version_tmp_A"

    # Check for equality or inequality as necessary.
    m4_case(m4_tolower(m4_substr($2,0,2)),
    [eq],[
      test "x$A" = "x$B" && ax_compare_version=true
    ],
    [ne],[
      test "x$A" != "x$B" && ax_compare_version=true
    ],[
      AC_WARNING([illegal OP parameter: $2])
    ])
  ])

  AS_VAR_POPDEF([A])dnl
  AS_VAR_POPDEF([B])dnl

  dnl # Execute ACTION-IF-TRUE / ACTION-IF-FALSE.
  if test "$ax_compare_version" = "true" ; then
    m4_ifvaln([$4],[$4],[:])dnl
    m4_ifvaln([$5],[else $5])dnl
  fi
]) dnl AX_COMPARE_VERSION

dnl #########################################################################
dnl Turn on the additional warnings last, so -Werror doesn't affect other tests.
AC_DEFUN([IDT_COMPILE_WARNINGS],[
    if test -f $srcdir/autogen.sh; then
	default_compile_warnings="maximum"
    else
	default_compile_warnings="no"
    fi

    AC_ARG_WITH(compile-warnings,
                AS_HELP_STRING([--with-compile-warnings=@<:@no/yes/maximum/error@:>@],
                               [Compiler warnings]),
                [enable_compile_warnings="$withval"],
                [enable_compile_warnings="$default_compile_warnings"])

    warnCFLAGS=
    if test "x$GCC" != xyes; then
	enable_compile_warnings=no
    fi

    warning_cflags=
    warning_valaflags=
    realsave_CFLAGS="$CFLAGS"

    # Everything from -Wall except:
    # 1. the -Wunused-* stuff
    # 2. the non C warnings: -Wreorder -Wc++11-compat
    # 3. unfixable issues: -Wmissing-braces
    #
    # We don't want to see warnings about generated code.
    CC_CHECK_FLAGS_APPEND([common_cflags], [CFLAGS], [\
        -Waddress \
        -Warray-bounds \
        -Wchar-subscripts \
        -Wenum-compare \
        -Wimplicit-int \
        -Wimplicit-function-declaration \
        -Wcomment \
        -Wformat \
        -Wmain \
        -Wmaybe-uninitialized \
        -Wnonnull \
        -Wparentheses \
        -Wpointer-sign \
        -Wreturn-type \
        -Wsequence-point \
        -Wsign-compare \
        -Wstrict-aliasing \
        -Wstrict-overflow=1 \
        -Wswitch \
        -Wtrigraphs \
        -Wuninitialized \
        -Wunknown-pragmas \
        -Wvolatile-register-var \
        ])

    case "$enable_compile_warnings" in
    no)
	warning_cflags=
	warning_valaflags=
	;;
    yes)
        CC_CHECK_FLAGS_APPEND([additional_cflags], [CFLAGS], [\
            -Wall \
            -Wunused \
            -Wmissing-prototypes \
            -Wmissing-declarations \
        ])

        CC_CHECK_FLAGS_APPEND([additional_valaflags], [CFLAGS], [\
            -Wmissing-prototypes \
            -Wmissing-declarations \
        ])

        dnl -Wall includes the $common_cflags already.
	warning_cflags="$additional_cflags"
	warning_valaflags="$common_cflags $additional_valaflags"
	;;
    maximum|error)
        CC_CHECK_FLAGS_APPEND([additional_cflags], [CFLAGS], [\
            -Wall \
            -Wunused \
            -Wchar-subscripts \
            -Wmissing-prototypes \
            -Wmissing-declarations \
            -Wnested-externs \
            -Wpointer-arit \
            -Wno-sign-compare \
            -Wno-pointer-sign \
        ])

        CC_CHECK_FLAGS_APPEND([additional_valaflags], [CFLAGS], [\
            -Wmissing-prototypes \
            -Wmissing-declarations \
            -Wnested-externs \
            -Wpointer-arith \
            -Wno-sign-compare \
            -Wno-pointer-sign \
        ])

        dnl -Wall includes the $common_cflags already.
	warning_cflags="$additional_cflags"
	warning_valaflags="$common_cflags $additional_valaflags"

	if test "$enable_compile_warnings" = "error" ; then
	    warning_cflags="$warning_cflags -Werror"
	    warning_valaflags="$warning_valaflags -Werror"
	fi
	;;
    *)
	AC_MSG_ERROR(Unknown argument '$enable_compile_warnings' to --with-compile-warnings)
	;;
    esac
    CFLAGS="$realsave_CFLAGS"
    AC_MSG_CHECKING(what warning flags to pass to the C compiler)
    AC_MSG_RESULT($warning_cflags)

    AC_MSG_CHECKING(what warning flags to pass to the C compiler for Vala built sources)
    AC_MSG_RESULT($warning_valaflags)

    WARN_CFLAGS="$warning_cflags"
    AC_SUBST(WARN_CFLAGS)

    WARN_VALACFLAGS="$warning_valaflags"
    AC_SUBST(WARN_VALACFLAGS)
]) dnl IDT_COMPILE_WARNINGS



dnl Stolen from https://git.gnome.org/browse/glib/tree/m4macros/glibtests.m4

dnl GLIB_TESTS
dnl

AC_DEFUN([GLIB_TESTS],
[
  AC_ARG_ENABLE(installed-tests,
                AS_HELP_STRING([--enable-installed-tests],
                               [Enable installation of some test cases]),
                [case ${enableval} in
                  yes) ENABLE_INSTALLED_TESTS="1"  ;;
                  no)  ENABLE_INSTALLED_TESTS="" ;;
                  *) AC_MSG_ERROR([bad value ${enableval} for --enable-installed-tests]) ;;
                 esac])
  AM_CONDITIONAL([ENABLE_INSTALLED_TESTS], test "$ENABLE_INSTALLED_TESTS" = "1")
  AC_ARG_ENABLE(always-build-tests,
                AS_HELP_STRING([--enable-always-build-tests],
                               [Enable always building tests during 'make all']),
                [case ${enableval} in
                  yes) ENABLE_ALWAYS_BUILD_TESTS="1"  ;;
                  no)  ENABLE_ALWAYS_BUILD_TESTS="" ;;
                  *) AC_MSG_ERROR([bad value ${enableval} for --enable-always-build-tests]) ;;
                 esac])
  AM_CONDITIONAL([ENABLE_ALWAYS_BUILD_TESTS], test "$ENABLE_ALWAYS_BUILD_TESTS" = "1")
  if test "$ENABLE_INSTALLED_TESTS" = "1"; then
    AC_SUBST(installed_test_metadir, [${datadir}/installed-tests/]AC_PACKAGE_NAME)
    AC_SUBST(installed_testdir, [${libexecdir}/installed-tests/]AC_PACKAGE_NAME)
  fi
])
