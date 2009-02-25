include $(top_srcdir)/Makefile.decl

module_flags = -module -avoid-version -no-undefined
indexer_modulesdir = $(libdir)/tracker/indexer-modules

INCLUDES =								\
	-DSHAREDIR=\""$(datadir)"\"					\
	-DLIBDIR=\""$(libdir)"\"					\
	-DLOCALEDIR=\""$(localedir)"\" 					\
	-DINDEXER_MODULES_DIR=\"$(indexer_modulesdir)\"			\
	-DG_LOG_DOMAIN=\"Tracker\"					\
	-D_GNU_SOURCE							\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	$(WARN_CFLAGS)							\
	$(GLIB2_CFLAGS)							\
	$(GCOV_CFLAGS)							\
	$(GMODULE_CFLAGS)						\
	$(GIO_CFLAGS)							\
	$(GCONF_CFLAGS)							\
	$(GMIME_CFLAGS)							\
	$(SQLITE3_CFLAGS)

indexer_modules_LTLIBRARIES = 						\
	libtracker-module-applications.la				\
	libtracker-module-files.la					\
	libtracker-module-gaim-conversations.la

# Applications module
libtracker_module_applications_la_SOURCES = applications.c
libtracker_module_applications_la_LDFLAGS = $(module_flags)
libtracker_module_applications_la_LIBADD = 				\
	$(GMODULE_LIBS)							\
	$(GIO_LIBS)							\
	$(GCOV_LIBS)							\
	$(GLIB2_LIBS)

# Files module
libtracker_module_files_la_SOURCES = files.c
libtracker_module_files_la_LDFLAGS = $(module_flags)
libtracker_module_files_la_LIBADD = 					\
	$(GMODULE_LIBS)							\
	$(GIO_LIBS)							\
	$(GCOV_LIBS)							\
	$(GLIB2_LIBS)

# Instant messaging module
libtracker_module_gaim_conversations_la_SOURCES = gaim-conversations.c
libtracker_module_gaim_conversations_la_LDFLAGS = $(module_flags)
libtracker_module_gaim_conversations_la_LIBADD = 			\
	$(GMODULE_LIBS)							\
	$(GIO_LIBS)							\
	$(GCOV_LIBS)							\
	$(GLIB2_LIBS)

if HAVE_GCONF

indexer_modules_LTLIBRARIES += 						\
	libtracker-module-evolution.la

# Evolution
libtracker_module_evolution_la_SOURCES =				\
	evolution-common.c						\
	evolution-common.h						\
	evolution-imap-db.c						\
	evolution-imap-db.h						\
	evolution-imap.c						\
	evolution-imap.h						\
	evolution-pop.c							\
	evolution-pop.h							\
	evolution.c

libtracker_module_evolution_la_LDFLAGS = $(module_flags)
libtracker_module_evolution_la_LIBADD = 				\
	$(GMODULE_LIBS)							\
	$(GLIB2_LIBS)							\
	$(GCONF_LIBS)							\
	$(GMIME_LIBS)							\
	$(GCOV_LIBS)							\
	$(SQLITE3_LIBS)

endif
