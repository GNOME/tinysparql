include $(top_srcdir)/Makefile.decl

AUTOMAKE_OPTIONS = 1.6

# The name of the module.
DOC_MODULE=libtracker-module

# The top-level SGML file.
DOC_MAIN_SGML_FILE=libtracker-module-docs.sgml

# Extra options to supply to gtkdoc-scan
# SCAN_OPTIONS=--deprecated-guards="G_GNUC_DEPRECATED"

# Extra options to pass to gtkdoc-scangobj
# SCANGOBJ_OPTIONS=--type-init-func="gtk_type_init(0)" 

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=../../../src/tracker-indexer

# Used for dependencies
HFILE_GLOB=$(top_srcdir)/src/tracker-indexer/*.h
CFILE_GLOB=$(top_srcdir)/src/tracker-indexer/*.c

# Header files to ignore when scanning
IGNORE_HFILES=								\
	tracker-dbus.c							\
	tracker-dbus.h							\
	tracker-indexer.c						\
	tracker-indexer-db.c						\
	tracker-indexer-db.h						\
	tracker-indexer-glue.h						\
	tracker-indexer-module.c					\
	tracker-indexer-module.h					\
	tracker-indexer.h						\
	tracker-main.c							\
	tracker-marshal.c						\
	tracker-marshal.h						\
	tracker-marshal-main.c


# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
INCLUDES = 								\
	-I$(top_srcdir)/src						\
	$(GMODULE_CFLAGS)						\
	$(PANGO_CFLAGS)							\
	$(DBUS_CFLAGS)

GTKDOC_LIBS =								\
	$(top_builddir)/src/libtracker-db/libtracker-db.la 		\
	$(top_builddir)/src/libtracker-common/libtracker-common.la 	\
	$(top_builddir)/src/libstemmer/libstemmer.la	 		\
	$(top_builddir)/src/tracker-indexer/libtracker-module.la	\
	$(trackerd_win_libs)						\
	$(DBUS_LIBS)							\
	$(GMODULE_LIBS)							\
	$(GTHREAD_LIBS)							\
	$(PANGO_LIBS)							\
	$(GIO_LIBS)							\
	$(GLIB2_LIBS)							\
	-lz								\
	-lm

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--sgml-mode --output-format=xml

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files =					\
	version.xml				\
	adding-categories-and-properties.sgml	\
	writing-an-extractor.sgml		\
	writing-an-indexer-module.sgml

expand_content_files =				\
	adding-categories-and-properties.sgml	\
	writing-an-extractor.sgml		\
	writing-an-indexer-module.sgml

# Images to copy into HTML directory
HTML_IMAGES =

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=

include $(top_srcdir)/gtk-doc.make

# Other files to distribute
EXTRA_DIST += version.xml.in
