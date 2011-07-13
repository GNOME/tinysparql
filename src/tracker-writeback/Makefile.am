include $(top_srcdir)/Makefile.decl

module_flags = -module -avoid-version -no-undefined
modulesdir = $(libdir)/tracker-$(TRACKER_API_VERSION)/writeback-modules

AM_CPPFLAGS =                                          \
	$(BUILD_CFLAGS)                                \
	-I$(top_srcdir)/src                            \
	-I$(top_builddir)/src                          \
	-DLOCALEDIR=\""$(localedir)"\"                 \
	-DMODULESDIR=\"$(modulesdir)\"                 \
	-DWRITEBACK_MODULES_DIR=\""$(modulesdir)"\"    \
	$(TRACKER_WRITEBACK_CFLAGS)                    \
	$(LIBTRACKER_MINER_CFLAGS)


modules_LTLIBRARIES =

# Audio files (MP3, MPC, FLAC, MP4, ASF, AIFF, WAV, TrueAudio, WavPack,
# Ogg FLAC, Ogg Vorbis and Speex)
if HAVE_TAGLIB
modules_LTLIBRARIES += libwriteback-taglib.la
libwriteback_taglib_la_SOURCES = tracker-writeback-taglib.c
libwriteback_taglib_la_LDFLAGS = $(module_flags)
libwriteback_taglib_la_LIBADD = $(BUILD_LIBS) $(GLIB2_LIBS) $(TAGLIB_LIBS)
endif

# XMP
if HAVE_EXEMPI
modules_LTLIBRARIES += libwriteback-xmp.la
libwriteback_xmp_la_SOURCES = tracker-writeback-xmp.c
libwriteback_xmp_la_LDFLAGS = $(module_flags)
libwriteback_xmp_la_LIBADD = $(BUILD_LIBS) $(GLIB2_LIBS) $(EXEMPI_LIBS)
endif

# Playlists
#if HAVE_TOTEM_PL_PARSER
#modules_LTLIBRARIES += libwriteback-playlist.la
#libwriteback_playlist_la_SOURCES = tracker-writeback-playlist.c
#libwriteback_playlist_la_LDFLAGS = $(module_flags)
#libwriteback_playlist_la_LIBADD = $(BUILD_LIBS) $(GLIB2_LIBS) $(TOTEM_PL_PARSER_LIBS)
#endif

#
# Binaries
#
libexec_PROGRAMS = tracker-writeback

tracker_writeback_SOURCES = \
	$(marshal_sources) \
	$(dbus_sources) \
	tracker-config.c \
	tracker-config.h \
	tracker-writeback-dbus.h \
	tracker-writeback-file.c \
	tracker-writeback-file.h \
	tracker-writeback-module.c \
	tracker-writeback-module.h \
	tracker-writeback.c \
	tracker-writeback.h \
	tracker-main.c

tracker_writeback_LDADD = \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(BUILD_LIBS) \
	$(TRACKER_WRITEBACK_LIBS) \
	$(LIBTRACKER_MINER_LIBS)


marshal_sources = \
	tracker-marshal.h \
	tracker-marshal.c

tracker-marshal.h: tracker-marshal.list
	$(AM_V_GEN)$(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --header > $@

tracker-marshal.c: tracker-marshal.list
	$(AM_V_GEN)echo "#include \"tracker-marshal.h\"" > $@ && \
		$(GLIB_GENMARSHAL) $< --prefix=tracker_marshal --body >> $@

BUILT_SOURCES = \
	$(marshal_sources)

CLEANFILES = $(BUILT_SOURCES)

EXTRA_DIST = tracker-marshal.list

