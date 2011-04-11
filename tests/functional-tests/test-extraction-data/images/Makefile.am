include $(top_srcdir)/Makefile.decl

imagedir = $(datadir)/tracker-tests/test-extraction-data/images

image_DATA = \
	test-image-2.png \
	test-image-2.expected

gifs = \
	xmp-loaded-1.gif \
	xmp-loaded-1.expected \
	comment-extension-block.expected \
	comment-extension-block.gif \
	corrupted-image.gif \
	corrupted-image.expected

tiffs = \
	test-image-3.tif \
	test-image-3.expected

jpegs = \
	test-image-1.jpg \
	test-image-1.expected \
	test-iptcdata-records.jpg \
	test-iptcdata-records.expected

if HAVE_LIBGIF
image_DATA += $(gifs)
endif

if HAVE_LIBTIFF
image_DATA += $(tiffs)
endif

if HAVE_LIBJPEG
image_DATA += $(jpegs)
endif

EXTRA_DIST = \
	$(image_DATA) \
	$(gifs) \
	$(tiffs \
	$(jpegs)
