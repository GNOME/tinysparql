AUTOMAKE_OPTIONS = 1.6

# Generation of the additional PNG files from DIA files
#  (list of original '.dia' files with '.png' extension)
ONTOLOGY_DIAGRAMS =                                    \
	$(top_srcdir)/docs/ontologies/notation.dia     \
	$(top_srcdir)/docs/ontologies/mfo/feeds-overview.dia \
	$(top_srcdir)/docs/ontologies/nmm/radio-overview.dia \
	$(top_srcdir)/docs/ontologies/nmm/images-overview.dia \
	$(top_srcdir)/docs/ontologies/mlo/location-overview.dia \
	$(top_srcdir)/docs/ontologies/nie/overview.dia \
	$(top_srcdir)/docs/ontologies/nmo/message-class-overview.dia \
	$(top_srcdir)/docs/ontologies/nmo/email-mimeparts-overview.dia

# The wanted PNG files
ONTOLOGY_DIAGRAMS_PNG = $(notdir $(ONTOLOGY_DIAGRAMS:.dia=.png))

# DIA to PNG conversion
update-diagrams: $(ONTOLOGY_DIAGRAMS)
	$(AM_V_GEN) dia -t png $^ 2>/dev/null

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
	mkdir xml
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
	$(ONTOLOGY_DIAGRAMS)                           \
	$(ONTOLOGY_EXPLANATIONS)

CLEANFILES +=                                          \
	gen-doc.stamp
