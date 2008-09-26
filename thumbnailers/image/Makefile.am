include $(top_srcdir)/Makefile.decl

thumbappbindir = $(libdir)/tracker/thumbnailers/image

if HAVE_IMAGEMAGICK
thumbappbin_SCRIPTS = 		\
	png_thumbnailer		\
	jpeg_thumbnailer 	\
	gif_thumbnailer		\
	tiff_thumbnailer
else
if HAVE_HILDON_THUMBNAIL
thumbappbin_SCRIPTS = 		\
	hildon/png_thumbnailer 	\
	hildon/jpeg_thumbnailer \
	hildon/gif_thumbnailer	\
	hildon/tiff_thumbnailer
endif
endif

EXTRA_DIST = $(thumbappbin_SCRIPTS)
