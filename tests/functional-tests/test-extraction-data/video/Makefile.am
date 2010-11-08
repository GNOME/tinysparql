include $(top_srcdir)/Makefile.decl

videotestdir = $(datadir)/tracker-tests/test-extraction-data/video

videotest_DATA =

# This should be GSTREAMER OR LIBXINE OR GSTREMER_HELIX OR USING_EXTERNAL_VIDEO_PLAYER
if HAVE_GSTREAMER
videotest_DATA +=                                       \
	video-1.mp4                                    	\
	video-1.expected                               	\
	video-2.mov                                    	\
	video-2.expected
endif

EXTRA_DIST = $(videotest_DATA)
