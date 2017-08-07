AUTOMAKE_OPTIONS = 1.6

# Generation of the base ontology XML files.
ontology-doc.stamp:
	$(MKDIR_P) xml
	$(top_builddir)/docs/tools/ttl2sgml -d $(top_srcdir)/src/ontologies/ -o xml/
	$(AM_V_GEN) touch $@

version.xml: ontology-doc.stamp

# The name of the module.
DOC_MODULE = libtracker-sparql

# The top-level SGML file.
DOC_MAIN_SGML_FILE = libtracker-sparql-docs.sgml

# Extra options to supply to gtkdoc-scan
SCAN_OPTIONS=--deprecated-guards="TRACKER_DISABLE_DEPRECATED"

# Extra options to pass to gtkdoc-scangobj
# SCANGOBJ_OPTIONS=--type-init-func="gtk_type_init(0)"

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR = $(top_srcdir)/src/libtracker-sparql

# Used for dependencies
HFILE_GLOB = $(top_srcdir)/src/libtracker-sparql/*.h $(top_builddir)/config.h
CFILE_GLOB = $(top_srcdir)/src/libtracker-sparql/*.c

# Header files to ignore when scanning
IGNORE_HFILES = \
	$(top_srcdir)/src/libtracker-sparql/tracker-generated-no-checks.h	\
	$(top_srcdir)/src/libtracker-sparql/tracker-uri.h

# CFLAGS and LDFLAGS for compiling scan program. Only needed
# if $(DOC_MODULE).types is non-empty.
AM_CPPFLAGS =                                          \
	-DTRACKER_COMPILATION                          \
	-I$(top_srcdir)/src                            \
	$(LIBTRACKER_SPARQL_CFLAGS)

GTKDOC_LIBS =                                          \
	$(top_builddir)/src/libtracker-sparql-backend/libtracker-sparql-@TRACKER_API_VERSION@.la \
	$(top_builddir)/src/libtracker-common/libtracker-common.la \
	$(LIBTRACKER_SPARQL_LIBS)

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS = --sgml-mode --output-format=xml

# Extra options to supply to gtkdoc-mkhtml
MKHTML_OPTIONS=--path="$(abs_builddir)"

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files =	version.xml examples.sgml overview.sgml ontologies.sgml private-store.xml migrating-1to2.xml

expand_content_files =

# Images to copy into HTML directory
HTML_IMAGES =

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS = \
        --extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/gobject \
        --extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/glib \
        --extra-dir=$(GLIB_PREFIX)/share/gtk-doc/html/gio

include $(top_srcdir)/gtk-doc.make

EXAMPLES = \
	examples/ontologies/defining-cardinality-1.rq \
	examples/ontologies/defining-cardinality-2.txt \
	examples/ontologies/defining-cardinality-3.rq \
	examples/ontologies/defining-classes-1.txt \
	examples/ontologies/defining-classes-2.txt \
	examples/ontologies/defining-classes-3.rq \
	examples/ontologies/defining-fts-indexes-1.txt \
	examples/ontologies/defining-fts-indexes-2.rq \
	examples/ontologies/defining-indexes-1.txt \
	examples/ontologies/defining-namespaces-1.txt \
	examples/ontologies/defining-properties-1.txt \
	examples/ontologies/defining-properties-2.txt \
	examples/ontologies/defining-properties-3.txt \
	examples/ontologies/defining-properties-4.rq \
	examples/ontologies/defining-uniqueness-1.txt \
	examples/ontologies/defining-uniqueness-2.rq \
	examples/ontologies/example.description \
	examples/ontologies/predefined-elements-1.txt \
	examples/ontologies/predefined-elements-2.rq

# Other files to distribute
EXTRA_DIST += \
	$(EXAMPLES) \
	version.xml.in \
	meson.build

CLEANFILES += \
	ontology-doc.stamp
