include ../Makefile-plugins.am

plugin_LTLIBRARIES = \
 libextractor_pdf.la

libextractor_pdf_la_LINK = \
  /bin/sh ../../../libtool --mode=link $(CXXLD) -o libextractor_pdf.la
libextractor_pdf_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand) \
  $(XTRA_CPPLIBS)
libextractor_pdf_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la \
  $(top_builddir)/src/plugins/libconvert.la \
  -lm 

libextractor_pdf_la_SOURCES = \
 aconf.h \
 aconf2.h \
 ErrorCodes.h \
 Stream-CCITT.h\
 gtypes.h \
 Array.cc Array.h \
 Catalog.cc Catalog.h \
 Decrypt.cc Decrypt.h \
 Dict.cc Dict.h \
 Error.cc Error.h\
 FontEncoding.cc FontEncoding.h\
 FontFile.cc FontFile.h StdFontInfo.h CompactFontInfo.h \
 Function.cc Function.h\
 GString.cc GString.h\
 Lexer.cc Lexer.h \
 Link.cc Link.h\
 Object.cc Object.h\
 PDFDoc.cc PDFDoc.h \
 Page.cc Page.h\
 Params.cc Params.h\
 Parser.cc Parser.h\
 Stream.cc Stream.h Stream-CCITT.h \
 XRef.cc XRef.h\
 gfile.cc gfile.h\
 gmem.cc gmem.h\
 gmempp.cc\
 parseargs.cc parseargs.h \
 pdfextractor.cc

# gcc 3.3 produces BROKEN code for -O1 and -O2 (PDF extraction
# would fail silently) hence we MUST override the user flag here
# which may contain -O1 or -O2!
CXXFLAGS = -O0

