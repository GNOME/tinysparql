include $(top_srcdir)/Makefile.decl

SUBDIRS = data src filters docs po python utils

if HAVE_UNIT_TESTS
SUBDIRS += tests
endif

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
	rdf-query-examples/80s-music.rdf 	\
	rdf-query-examples/90s-music.rdf 	\
	rdf-query-examples/all-documents.rdf 	\
	rdf-query-examples/big-documents.rdf 	\
	rdf-query-examples/wallpapers.rdf 	\
	intltool-extract.in 			\
	intltool-merge.in 			\
	intltool-update.in 

DISTCLEANFILES = 				\
	intltool-extract 			\
	intltool-merge 				\
	intltool-update

DISTCHECK_CONFIGURE_FLAGS =			\
	--with-session-bus-services-dir="\$(datadir)"/dbus-1/services \
	--disable-deskbar-applet		\
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

