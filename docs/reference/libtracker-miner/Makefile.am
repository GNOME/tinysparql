include $(top_srcdir)/Makefile.decl

AUTOMAKE_OPTIONS = 1.6

# The name of the module.
DOC_MODULE=libtracker-miner

# The top-level SGML file.
DOC_MAIN_SGML_FILE=libtracker-miner-docs.sgml

# Extra options to supply to gtkdoc-scan
# SCAN_OPTIONS=--deprecated-guards="G_GNUC_DEPRECATED"

# Extra options to pass to gtkdoc-scangobj
# SCANGOBJ_OPTIONS=--type-init-func="gtk_type_init(0)"

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=../../../src/libtracker-miner

# Used for dependencies
HFILE_GLOB=$(top_srcdir)/src/libtracker-miner/*.h
CFILE_GLOB=$(top_srcdir)/src/libtracker-miner/*.c

# Header files to ignore when scanning
IGNORE_HFILES=								\
	tracker-crawler.h						\
	tracker-dbus.h							\
	tracker-marshal.h						\
	tracker-miner-client.h						\
	tracker-miner-dbus.h						\
	tracker-miner-glue.h						\
	tracker-miner-web-dbus.h					\
	tracker-miner-web-glue.h					\
	tracker-miner-web-glue.h					\
	tracker-monitor.h						\
	tracker-utils.h

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
INCLUDES = 								\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	$(GLIB2_CFLAGS)							\
	$(GIO_CFLAGS)							\
	$(DBUS_CFLAGS)

GTKDOC_LIBS =								\
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la	 \
	$(top_builddir)/src/libtracker-client/libtracker-client-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la	\
	$(DBUS_LIBS)							\
	$(GIO_LIBS)							\
	$(GLIB2_LIBS)


# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--sgml-mode --output-format=xml

# Extra options to supply to gtkdoc-mkhtml
MKHTML_OPTIONS=--path="$(abs_builddir)"

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files =						\
	version.xml

expand_content_files =

# Images to copy into HTML directory
HTML_IMAGES =

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS=

include $(top_srcdir)/gtk-doc.make

# Other files to distribute
EXTRA_DIST += version.xml.in
