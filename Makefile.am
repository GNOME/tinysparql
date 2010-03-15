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
		tracker-sparql-query.c \
		tracker-sparql-scanner.c \
		tracker-turtle-reader.c \
		tracker-turtle-writer.c \
		"*.vapi" \
		"*-glue.h"
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
	--disable-evolution-miner		\
	--disable-deskbar-applet		\
	--disable-nautilus-extension		\
	--enable-gtk-doc

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
