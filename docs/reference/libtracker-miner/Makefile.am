AUTOMAKE_OPTIONS = 1.6

# The name of the module.
DOC_MODULE=libtracker-miner

# The top-level SGML file.
DOC_MAIN_SGML_FILE=libtracker-miner-docs.sgml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards="TRACKER_DISABLE_DEPRECATED"

# Extra options to pass to gtkdoc-scangobj
# SCANGOBJ_OPTIONS=--type-init-func="gtk_type_init(0)"

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=$(top_srcdir)/src/libtracker-miner

# Used for dependencies
HFILE_GLOB=$(top_srcdir)/src/libtracker-miner/*.h
CFILE_GLOB=$(top_srcdir)/src/libtracker-miner/*.c

# Header files to ignore when scanning
IGNORE_HFILES=                                         \
	tracker-crawler.h                              \
	tracker-dbus.h                                 \
	tracker-file-notifier.h                        \
	tracker-file-system.h                          \
	tracker-miner-client.h                         \
	tracker-miner-dbus.h                           \
	tracker-miner-glue.h                           \
	tracker-miner-files-index-client.h             \
	tracker-miner-fs-processing-pool.h             \
	tracker-miner-web-dbus.h                       \
	tracker-miner-web-glue.h                       \
	tracker-miner-web-glue.h                       \
	tracker-monitor.h                              \
	tracker-priority-queue.h                       \
	tracker-sparql-buffer.h                        \
	tracker-task-pool.h                            \
	tracker-utils.h

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
AM_CPPFLAGS =                                          \
	-DTRACKER_COMPILATION                          \
	-I$(top_srcdir)/src                            \
	$(LIBTRACKER_MINER_CFLAGS)

GTKDOC_LIBS =                                          \
	$(top_builddir)/src/libtracker-miner/libtracker-miner-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(LIBTRACKER_MINER_LIBS)

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS=--sgml-mode --output-format=xml

# Extra options to supply to gtkdoc-mkhtml
MKHTML_OPTIONS=--path="$(abs_builddir)"

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files = version.xml overview.sgml

expand_content_files =

# Images to copy into HTML directory
HTML_IMAGES =

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS = \
	--extra-dir=../libtracker-control/html \
	--extra-dir=../libtracker-sparql/html \
        --extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/gobject \
        --extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/glib \
        --extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/gio

include $(top_srcdir)/gtk-doc.make

# Other files to distribute
EXTRA_DIST += version.xml.in
