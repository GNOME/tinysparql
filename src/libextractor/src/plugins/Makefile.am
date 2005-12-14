include Makefile-plugins.am

if HAVE_GLIB
oledir=ole2
if HAVE_GTK
thumbdir=thumbnail
endif
endif

if HAVE_PRINTABLE
 printdir=printable
endif

if HAVE_EXIV2
 exiv2dir=exiv2
endif

SUBDIRS = . oo $(printdir) hash $(oledir) rpm pdf $(thumbdir) $(exiv2dir)

if HAVE_VORBISFILE
 extraogg = libextractor_ogg.la
endif

if NEED_VORBIS
 vorbisflag = -lvorbis
endif

if HAVE_ZLIB
extraqt = libextractor_qt.la
endif

plugin_LTLIBRARIES = \
  libextractor_asf.la \
  libextractor_deb.la \
  libextractor_dvi.la \
  libextractor_elf.la \
  libextractor_filename.la \
  libextractor_gif.la \
  libextractor_html.la \
  libextractor_id3v2.la \
  libextractor_id3v24.la \
  libextractor_id3v23.la \
  libextractor_jpeg.la \
  libextractor_lower.la \
  libextractor_man.la \
  libextractor_mime.la \
  libextractor_mp3.la \
  libextractor_mpeg.la \
  $(extraogg) \
  libextractor_png.la \
  libextractor_ps.la \
  $(extraqt) \
  libextractor_real.la \
  libextractor_riff.la \
  libextractor_split.la \
  libextractor_tar.la \
  libextractor_tiff.la \
  libextractor_translit.la \
  libextractor_wav.la \
  libextractor_zip.la 

if HAVE_VORBISFILE
libextractor_ogg_la_SOURCES = \
  oggextractor.c
libextractor_ogg_la_LDFLAGS = \
 -lvorbisfile $(vorbisflag) $(PLUGINFLAGS)  $(retaincommand)
endif

noinst_LTLIBRARIES = \
  libpack.la \
  libconvert.la


if HAVE_ZLIB
libextractor_qt_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_qt_la_LIBADD = \
  -lz
libextractor_qt_la_SOURCES = \
  qtextractor.c 
endif

libextractor_ps_la_SOURCES = \
  psextractor.c
libextractor_ps_la_LDFLAGS = \
  $(top_builddir)/src/main/libextractor.la \
  $(PLUGINFLAGS)  $(retaincommand)

libextractor_wav_la_SOURCES = \
  wavextractor.c
libextractor_wav_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)

libextractor_mp3_la_SOURCES = \
  mp3extractor.c 
libextractor_mp3_la_LDFLAGS = \
  $(PLUGINFLAGS) $(retaincommand)
libextractor_mp3_la_LIBADD = \
  libconvert.la  

libextractor_id3v2_la_SOURCES = \
  id3v2extractor.c 
libextractor_id3v2_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_id3v2_la_LIBADD = \
  libconvert.la

libextractor_id3v23_la_SOURCES = \
  id3v23extractor.c 
libextractor_id3v23_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_id3v23_la_LIBADD = \
  libconvert.la 

libextractor_id3v24_la_SOURCES = \
  id3v24extractor.c 
libextractor_id3v24_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_id3v24_la_LIBADD = \
  libconvert.la

libextractor_dvi_la_SOURCES = \
  dviextractor.c 
libextractor_dvi_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)

libextractor_asf_la_SOURCES = \
  asfextractor.c 
libextractor_asf_la_LDFLAGS = \
  $(PLUGINFLAGS) $(retaincommand)

libextractor_tar_la_SOURCES = \
  tarextractor.c 
libextractor_tar_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_tar_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la -lz

libextractor_lower_la_SOURCES = \
  lowerextractor.c
libextractor_lower_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)

libextractor_gif_la_SOURCES = \
  gifextractor.c 
libextractor_gif_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_gif_la_LIBADD = \
  libpack.la

libextractor_tiff_la_SOURCES = \
  tiffextractor.c 
libextractor_tiff_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_tiff_la_LIBADD = \
  libpack.la

libextractor_elf_la_SOURCES = \
  elfextractor.c 
libextractor_elf_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_elf_la_LIBADD = \
  libpack.la

libpack_la_SOURCES = \
  pack.c \
  pack.h 

libconvert_la_SOURCES = \
  convert.c \
  convert.h 
libconvert_la_LDFLAGS = \
  $(LIBICONV)

libextractor_zip_la_SOURCES = \
  zipextractor.c
libextractor_zip_la_LDFLAGS = \
  $(top_builddir)/src/main/libextractor.la \
  $(PLUGINFLAGS)  $(retaincommand)

libextractor_jpeg_la_SOURCES = \
  jpegextractor.c 
libextractor_jpeg_la_LDFLAGS = \
  $(PLUGINFLAGS)   $(retaincommand)

libextractor_html_la_SOURCES = \
  htmlextractor.c 
libextractor_html_la_LDFLAGS = \
  $(PLUGINFLAGS)   $(retaincommand)
libextractor_html_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la \
  libconvert.la 

libextractor_real_la_SOURCES = \
  realextractor.c 
libextractor_real_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)

libextractor_mime_la_SOURCES = \
  mimeextractor.c 
libextractor_mime_la_LDFLAGS = \
  $(top_builddir)/src/main/libextractor.la \
  $(PLUGINFLAGS)  $(retaincommand)

libextractor_mpeg_la_SOURCES = \
  mpegextractor.c 
libextractor_mpeg_la_LDFLAGS = \
  $(top_builddir)/src/main/libextractor.la \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_mpeg_la_LIBADD = \
  -lm

libextractor_man_la_SOURCES = \
  manextractor.c 
libextractor_man_la_LDFLAGS = \
  $(top_builddir)/src/main/libextractor.la \
  $(PLUGINFLAGS) $(retaincommand)

libextractor_deb_la_SOURCES = \
  debextractor.c 
libextractor_deb_la_LDFLAGS = \
  $(top_builddir)/src/main/libextractor.la \
  $(PLUGINFLAGS) $(retaincommand)
libextractor_deb_la_LIBADD = \
  -lz

libextractor_riff_la_SOURCES = \
  riffextractor.c 
libextractor_riff_la_LDFLAGS = \
  $(top_builddir)/src/main/libextractor.la \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_riff_la_LIBADD = \
  -lm 

libextractor_png_la_SOURCES = \
  pngextractor.c
libextractor_png_la_LDFLAGS = \
  $(top_builddir)/src/main/libextractor.la \
  $(PLUGINFLAGS) $(retaincommand)
libextractor_png_la_LIBADD = \
  libconvert.la -lz

libextractor_filename_la_SOURCES = \
  filenameextractor.c 
libextractor_filename_la_LDFLAGS = \
  $(PLUGINFLAGS)  $(retaincommand)
libextractor_filename_la_LIBADD = \
  $(top_builddir)/src/main/libextractor.la \
  libconvert.la

libextractor_split_la_SOURCES = \
  splitextractor.c 
libextractor_split_la_LDFLAGS = \
  $(PLUGINFLAGS) $(retaincommand)

libextractor_translit_la_SOURCES = \
  translitextractor.c 
libextractor_translit_la_LDFLAGS = \
  $(PLUGINFLAGS) $(retaincommand)

