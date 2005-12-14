SUBDIRS  = .

INCLUDES = -I$(top_srcdir)/src/include $(LTDLINCL)

# install plugins under:
plugindir = $(libdir)/libextractor

LIBS = \
 @LTLIBINTL@ @LIBS@

lib_LTLIBRARIES = \
  libextractor.la 

bin_PROGRAMS = extract

AM_CFLAGS = \
  -DPLUGIN_PATH="\"${plugindir}\""

extract_LDADD = \
  $(top_builddir)/src/main/libextractor.la 

if !MINGW
  dlflag=-ldl
endif

#if HAVE_ZLIB
  zlib =-lz
#endif
#if HAVE_BZ2
  bz2lib = -lbz2
#endif

libextractor_la_LDFLAGS = \
  -export-dynamic -version-info 2:0:1 $(LIBICONV)
libextractor_la_LIBADD = \
  $(LIBLTDL) $(dlflag) $(zlib) $(bz2lib)
libextractor_la_DEPENDENCIES = \
  $(LIBLTDL) 


EXTRA_DIST = \
  winproc.c \
  iconv.c

if MINGW
  winproc = winproc.c
endif

libextractor_la_SOURCES = \
  extractor.c \
  $(winproc) 

extract_SOURCES = \
  extract.c \
  getopt.c \
  getopt.h \
  getopt1.c 


check_PROGRAMS = \
  test_binary

TESTS = $(check_PROGRAMS)

test_binary_SOURCES = \
  test_binary.c
test_binary_LDADD = \
  $(top_builddir)/src/main/libextractor.la

