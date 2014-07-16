include $(top_srcdir)/Makefile.decl

SUBDIRS = data src docs po utils examples

if HAVE_UNIT_TESTS
SUBDIRS += tests
endif

#if ENABLE_GCOV
.PHONY: coverage coverage-report
coverage:
	find $(top_srcdir) -name "*.gcda" | xargs rm -f
	$(MAKE) $(AM_MAKEFLAGS) test

coverage-report: coverage
	lcov --directory $(top_srcdir)/src --capture --output-file $(top_builddir)/lcov.info
	lcov --directory $(top_srcdir)/src --output-file $(top_builddir)/lcov.info --remove $(top_builddir)/lcov.info \
		tracker-sparql-builder.c               \
		tracker-sparql-expression.c            \
		tracker-sparql-pattern.c               \
		tracker-sparql-query.c                 \
		tracker-sparql-scanner.c               \
		tracker-turtle-reader.c                \
		tracker-turtle-writer.c                \
		libtracker-bus/tracker-array-cursor.c  \
		libtracker-bus/tracker-bus-fd-cursor.c \
		libtracker-bus/tracker-bus.c           \
		libtracker-direct/tracker-direct.c     \
		libtracker-miner/tracker-storage.c     \
		libtracker-miner/tracker-dbus.c        \
		libtracker-miner/tracker-miner-fs.c    \
		libtracker-miner/tracker-miner-object.c \
		libtracker-miner/tracker-miner-web.c   \
		libtracker-miner/tracker-network-provider.c \
		libtracker-miner/tracker-network-provider-network-manager.c \
		libtracker-sparql/tracker-builder.c    \
		libtracker-sparql/tracker-connection.c \
		libtracker-sparql/tracker-cursor.c     \
		libtracker-sparql/tracker-plugin-loader.c \
		libtracker-sparql/tracker-utils.c      \
		libtracker-sparql-backend/tracker-backend.c \
		tracker-store/tracker-backup.c \
		tracker-store/tracker-dbus.c \
		tracker-store/tracker-main.c \
		tracker-store/tracker-resources.c \
		tracker-store/tracker-statistics.c \
		tracker-store/tracker-status.c \
		tracker-store/tracker-steroids.c \
		tracker-store/tracker-store.c \
		tracker-control/* \
		libstemmer/stem_UTF_8_* \
		tracker-control/tracker-miner-files-index-client.h \
		"*.vapi"                               \
	rm -rf $(top_builddir)/coverage
	$(MKDIR_P) $(top_builddir)/coverage
	genhtml --title "@PACKAGE_STRING@" --output-directory $(top_builddir)/coverage $(top_builddir)/lcov.info
#endif

distclean-local:
	if test "x$(srcdir)" = "x."; then :; else \
		rm -f ChangeLog; \
	fi

.PHONY: ChangeLog
gen_start_date = 2009-04-10
ChangeLog:
	$(AM_V_GEN) if test -d "$(srcdir)/.git"; then \
	  $(top_srcdir)/gitlog-to-changelog --since=$(gen_start_date) > $@.tmp \
	  && mv -f $@.tmp $@ \
	  || ($(RM) $@.tmp; \
	      echo Failed to generate ChangeLog, your ChangeLog may be outdated >&2; \
	      (test -f $@ || echo git-log is required to generate this file >> $@)); \
	else \
	  test -f $@ || \
	  (echo A git checkout and git-log is required to generate ChangeLog >&2 && \
	  echo A git checkout and git-log is required to generate this file >> $@); \
	fi

functional-test:
	cd tests/functional-tests/ && $(MAKE) $(AM_MAKEFLAGS) $@

EXTRA_DIST +=                                          \
	ChangeLog.pre-0-6-93                           \
	gitlog-to-changelog                            \
	intltool-extract.in                            \
	intltool-merge.in                              \
	intltool-update.in			       \
	tracker.doap

DISTCLEANFILES +=                                      \
	intltool-extract                               \
	intltool-merge                                 \
	intltool-update

DISTCHECK_CONFIGURE_FLAGS =                            \
	--with-session-bus-services-dir="\$(datadir)"/dbus-1/services \
	--disable-nautilus-extension                   \
	--enable-unit-tests                            \
	--enable-functional-tests                      \
	--enable-gtk-doc                               \
	--enable-introspection                         \
	--disable-miner-rss                            \
	--disable-miner-evolution                      \
	--disable-miner-thunderbird                    \
	--disable-miner-firefox                        \
	--enable-poppler                               \
	--enable-exempi                                \
	--enable-libiptcdata                           \
	--enable-libjpeg                               \
	--enable-libtiff                               \
	--enable-libvorbis                             \
	--enable-libflac                               \
	--enable-libgsf                                \
	--enable-playlist                              \
	--enable-tracker-preferences                   \
	--with-enca

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
