SUBDIRS = .

ELIBDIR = $(libdir)

AM_CFLAGS = -DELIBDIR=\"$(ELIBDIR)\" -DPLUGIN_PATH="\"${libdir}\""

LDADD = \
 $(top_builddir)/src/main/libextractor.la  

check_PROGRAMS = \
 trivialtest \
 keywordlisttest \
 plugintest \
 multiload

TESTS = $(check_PROGRAMS)

plugintest_SOURCES = \
 plugintest.c 

trivialtest_SOURCES = \
 trivialtest.c 

keywordlisttest_SOURCES = \
 keywordlisttest.c 

multiload_SOURCES = \
 multiload.c 

