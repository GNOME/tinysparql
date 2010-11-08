include $(top_srcdir)/Makefile.decl

imagestestdir = $(datadir)/tracker-tests/test-extraction-data/images

imagestest_DATA =                                      \
	test-image-2.png                               \
	test-image-2.expected                          

if HAVE_LIBGIF
imagestest_DATA += 				       \
	xmp-loaded-1.gif                               \
	xmp-loaded-1.expected                          \
	comment-extension-block.expected	       \
	comment-extension-block.gif		       \
	corrupted-image.gif                    	       \
	corrupted-image.expected
endif

if HAVE_LIBTIFF
imagestest_DATA += 				       \
	test-image-3.tif                               \
	test-image-3.expected                          
endif

if HAVE_LIBJPEG
imagestest_DATA +=					\
	test-image-1.jpg                               	\
	test-image-1.expected                          	
endif	

EXTRA_DIST = $(imagestest_DATA)
