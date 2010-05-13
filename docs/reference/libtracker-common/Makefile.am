include $(top_srcdir)/Makefile.decl

AUTOMAKE_OPTIONS = 1.6

# The name of the module.
DOC_MODULE=libtracker-common

# The top-level SGML file.
DOC_MAIN_SGML_FILE=libtracker-common-docs.sgml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards="TRACKER_DISABLE_DEPRECATED"

# Extra options to pass to gtkdoc-scangobj
# SCANGOBJ_OPTIONS=--type-init-func="gtk_type_init(0)"

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR=../../../src/libtracker-common

# Used for dependencies
HFILE_GLOB=$(top_srcdir)/src/libtracker-common/*.h
CFILE_GLOB=$(top_srcdir)/src/libtracker-common/*.c

# Header files to ignore when scanning
IGNORE_HFILES=

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
INCLUDES = 								\
	-DTRACKER_COMPILATION						\
	-I$(top_srcdir)/src						\
	$(GLIB2_CFLAGS)							\
	$(GIO_CFLAGS)							\
	$(DBUS_CFLAGS)

GTKDOC_LIBS =								\
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
