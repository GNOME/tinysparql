noinst_PROGRAMS = ttl2sgml ttlresource2sgml

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-DTOP_SRCDIR=\"$(top_srcdir)\"                 \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	$(LIBTRACKER_DATA_CFLAGS)

LDADD =                                                \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(top_builddir)/src/libtracker-data/libtracker-data.la \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(LIBTRACKER_DATA_LIBS) \
	$(BUILD_LIBS)

TTL_LOADER_FILES =                                     \
	ttl_loader.c                                   \
	ttl_loader.h                                   \
	ttl_model.c                                    \
	ttl_model.h                                    \
	ttl_sgml.c                                     \
	ttl_sgml.h                                     \
	qname.c                                        \
	qname.h

ttl2sgml_SOURCES =                                     \
	$(TTL_LOADER_FILES)                            \
	ttl2sgml.c

ttlresource2sgml_SOURCES =                             \
	$(TTL_LOADER_FILES)                            \
	ttlresource2sgml.c

EXTRA_DIST = \
	gen-doc.sh \
	meson.build
