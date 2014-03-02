AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	-DSHAREDIR=\""$(datadir)"\"                    \
	-DTRACKER_EXTRACTOR_RULES_DIR=\""$(TRACKER_EXTRACT_RULES_DIR)"\" \
	-DTRACKER_EXTRACTORS_DIR=\""$(TRACKER_EXTRACT_MODULES_DIR)"\" \
	$(LIBTRACKER_EXTRACT_CFLAGS)

libtracker_extractdir = $(libdir)/tracker-$(TRACKER_API_VERSION)
libtracker_extract_LTLIBRARIES = libtracker-extract.la

libtracker_extract_la_LIBADD =   \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_EXTRACT_LIBS)

libtracker_extract_la_SOURCES =  \
	tracker-data.h                                 \
	tracker-encoding.c                             \
	tracker-encoding.h                             \
	tracker-exif.c                                 \
	tracker-exif.h                                 \
	tracker-extract.h                              \
	tracker-extract-client.c                       \
	tracker-extract-client.h                       \
	tracker-extract-info.c                         \
	tracker-extract-info.h                         \
	tracker-guarantee.c                            \
	tracker-guarantee.h                            \
	tracker-iptc.c                                 \
	tracker-iptc.h                                 \
	tracker-module-manager.c                       \
	tracker-module-manager.h                       \
	tracker-utils.c                                \
	tracker-utils.h                                \
	tracker-xmp.c                                  \
	tracker-xmp.h

noinst_HEADERS =                                       \
	tracker-data.h                                 \
	tracker-encoding.h                             \
	tracker-exif.h                                 \
	tracker-extract-client.h                       \
	tracker-extract-info.h                         \
	tracker-extract.h                              \
	tracker-guarantee.h                            \
	tracker-iptc.h                                 \
	tracker-module-manager.h                       \
	tracker-utils.h                                \
	tracker-xmp.h

if HAVE_ENCA
libtracker_extract_la_SOURCES += \
	tracker-encoding-enca.c                        \
	tracker-encoding-enca.h
endif

if HAVE_MEEGOTOUCH
libtracker_extract_la_SOURCES += \
	tracker-encoding-meegotouch.cpp                \
	tracker-encoding-meegotouch.h
endif

if HAVE_LIBICU_CHARSET_DETECTION
libtracker_extract_la_SOURCES += \
	tracker-encoding-libicu.c                      \
	tracker-encoding-libicu.h
AM_CPPFLAGS +=                                         \
	$(LIBICU_CFLAGS)
libtracker_extract_la_LIBADD +=  \
	$(LIBICU_LIBS)
endif
