include ../Makefile-plugins.am

SUBDIRS = .

plugin_LTLIBRARIES = \
 libextractor_exiv2.la

libextractor_exiv2_la_LINK = \
  /bin/sh ../../../libtool --mode=link $(CXXLD) -o libextractor_exiv2.la
libextractor_exiv2_la_LDFLAGS = \
  $(XTRA_CPPLIBS) -lpthread \
  $(PLUGINFLAGS) $(retaincommand)
libextractor_exiv2_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la -lpthread

libextractor_exiv2_la_SOURCES = \
exiv2extractor.cc exv_conf.h exv_msvc.h \
basicio.cpp basicio.hpp \
canonmn.cpp canonmn.hpp \
datasets.cpp datasets.hpp \
error.cpp error.hpp \
exif.cpp exif.hpp \
fujimn.cpp fujimn.hpp \
futils.cpp futils.hpp \
ifd.cpp ifd.hpp \
image.cpp image.hpp \
iptc.cpp iptc.hpp \
jpgimage.cpp jpgimage.hpp \
makernote.cpp makernote.hpp \
metadatum.cpp metadatum.hpp \
mn.hpp \
nikonmn.cpp nikonmn.hpp \
olympusmn.cpp olympusmn.hpp \
panasonicmn.cpp panasonicmn.hpp \
rcsid.hpp \
sigmamn.cpp sigmamn.hpp \
sonymn.cpp sonymn.hpp \
tags.cpp tags.hpp \
types.cpp types.hpp \
value.cpp value.hpp 

