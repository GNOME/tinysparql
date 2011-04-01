include $(top_srcdir)/Makefile.decl

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	-DLOCALEDIR=\""$(localedir)"\"                 \
	$(LIBTRACKER_SPARQL_CFLAGS)

noinst_PROGRAMS = mtp-dummy

mtp_dummy_SOURCES = mtp-dummy.c
mtp_dummy_LDADD =                                      \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS)                                  \
	$(LIBTRACKER_SPARQL_LIBS)
