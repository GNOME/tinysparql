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
		tracker-sparql-builder.c \
		tracker-sparql-expression.c \
		tracker-sparql-pattern.c \
		tracker-sparql-query.c \
		tracker-sparql-scanner.c \
		tracker-turtle-reader.c \
		tracker-turtle-writer.c \
		libtracker-miner/tracker-storage.c \
		libtracker-miner/tracker-dbus.c \
		libtracker-miner/tracker-miner-fs.c \
		libtracker-miner/tracker-miner-object.c \
		libtracker-miner/tracker-miner-web.c \
		libtracker-miner/tracker-network-provider.c \
		libtracker-miner/tracker-network-provider-network-manager.c \
		"*.vapi" \
		"*-glue.h" \
		"tracker-marshal.*" \
		"inotify-*.*"
	rm -rf $(top_builddir)/coverage
	$(mkdir_p) $(top_builddir)/coverage
	genhtml --title "@PACKAGE_STRING@" --output-directory $(top_builddir)/coverage $(top_builddir)/lcov.info
#endif

dist-hook: gen-ChangeLog

gen_start_date = 2009-04-10
.PHONY: gen-ChangeLog
gen-ChangeLog:
	if test -d .git; then						\
	  $(top_srcdir)/gitlog-to-changelog				\
	    --since=$(gen_start_date) > $(distdir)/cl-t;		\
	  rm -f $(distdir)/ChangeLog;					\
	  mv $(distdir)/cl-t $(distdir)/ChangeLog;			\
	fi

EXTRA_DIST = 					\
	ChangeLog.pre-0-6-93			\
	gitlog-to-changelog			\
	intltool-extract.in 			\
	intltool-merge.in 			\
	intltool-update.in

DISTCLEANFILES = 				\
	intltool-extract 			\
	intltool-merge 				\
	intltool-update

DISTCHECK_CONFIGURE_FLAGS =			\
	--with-session-bus-services-dir="\$(datadir)"/dbus-1/services \
	--disable-nautilus-extension		\
	--enable-unit-tests			\
	--enable-functional-tests		\
	--enable-gtk-doc			\
	--enable-miner-rss			\
	--enable-miner-flickr			\
	--disable-miner-evolution		\
	--enable-poppler			\
	--enable-exempi				\
	--enable-libiptcdata			\
	--enable-libjpeg			\
	--enable-libtiff			\
	--enable-libvorbis			\
	--enable-libflac			\
	--enable-libgsf				\
	--enable-playlist			\
	--enable-tracker-preferences		\
	--enable-tracker-search-bar		\
	--enable-tracker-status-icon		\
	--with-enca

if OLD_EXEC_REMOVE_ALL
install-exec-hook:
	rm -Rf $(DESTDIR)$(bindir)/trackerd
	rm -Rf $(DESTDIR)$(bindir)/tracker-indexer
	rm -Rf $(DESTDIR)$(bindir)/tracker-thumbnailer
	rm -Rf $(DESTDIR)$(bindir)/tracker-extract
endif

if OLD_DATA_REMOVE_ALL
install-data-hook:
	rm -Rf $(DESTDIR)$(DBUS_SERVICES_DIR)/tracker.service
	rm -Rf $(DESTDIR)$(datadir)/tracker/sqlite-service-stored-procs.sql
	rm -Rf $(DESTDIR)$(datadir)/tracker/tracker-introspect.xml
endif

ACLOCAL_AMFLAGS = -I m4
