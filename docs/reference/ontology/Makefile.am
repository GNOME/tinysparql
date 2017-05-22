AUTOMAKE_OPTIONS = 1.6

# The original Ontology explanation files
ONTOLOGY_EXPLANATIONS =                                \
	$(top_srcdir)/docs/ontologies/mfo/explanation.xml \
	$(top_srcdir)/docs/ontologies/nmm/explanation.xml \
	$(top_srcdir)/docs/ontologies/mlo/explanation.xml \
	$(top_srcdir)/docs/ontologies/nie/explanation.xml \
	$(top_srcdir)/docs/ontologies/nmm/explanation.xml \
	$(top_srcdir)/docs/ontologies/nco/explanation.xml \
	$(top_srcdir)/docs/ontologies/nmo/explanation.xml

# Generation of the ontology XML files.
gen-doc.stamp: $(ONTOLOGY_EXPLANATIONS)
	$(MKDIR_P) xml
	$(top_srcdir)/docs/tools/gen-doc.sh $(top_builddir)/docs/tools/ttl2sgml $(top_builddir)/docs/tools/ttlresource2sgml $(top_srcdir)/src/ontologies $(top_srcdir)/docs/ontologies xml/
	$(AM_V_GEN) touch $@

version.xml: gen-doc.stamp

# The name of the module.
DOC_MODULE = ontology

# The top-level SGML file.
DOC_MAIN_SGML_FILE = ontology-docs.sgml

# The directory containing the source code. Relative to $(srcdir)
DOC_SOURCE_DIR = .

# Extra options to supply to gtkdoc-mkdb
MKDB_OPTIONS = --sgml-mode --output-format=xml

# Extra options to supply to gtkdoc-mkhtml
MKHTML_OPTIONS=--path="$(abs_builddir)"

# Extra SGML files that are included by $(DOC_MAIN_SGML_FILE)
content_files =                                        \
	version.xml

expand_content_files =

# Extra options to supply to gtkdoc-fixref
FIXXREF_OPTIONS =

include $(top_srcdir)/gtk-doc.make

# Other files to distribute
# Note that HTML_FILES and content_files are already included
#  by gtk-doc
EXTRA_DIST +=                                          \
	version.xml.in                                 \
	$(ONTOLOGY_EXPLANATIONS)                       \
	meson.build

CLEANFILES +=                                          \
	gen-doc.stamp
