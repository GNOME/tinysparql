
#include "config.h"

#include <stdio.h>
#include <string.h>
#include <glib.h>

gchar *video_tags[][2] = {
      { "ID_VIDEO_HEIGHT",  "Video.Height"     },
      { "ID_VIDEO_WIDTH",   "Video.Width"      },
      { "ID_VIDEO_FPS",     "Video.FrameRate"  },
      { "ID_VIDEO_CODEC",   "Video.Codec"      },
      { "ID_VIDEO_BITRATE", "Video.Bitrate"    },
      { NULL,               NULL               }
   };

gchar *audio_tags[][2] = {
      { "ID_AUDIO_BITRATE", "Audio.Bitrate"    },
      { "ID_AUDIO_RATE",    "Audio.Samplerate" },
      { "ID_AUDIO_CODEC",   "Audio.Codec"      },
      { "ID_AUDIO_NCH",     "Audio.Channels"   },
      { NULL,               NULL               }
   };

gchar *info_tags[][2] = {
      { "comments",     "Video.Comment"        },
      { "name",         "Video.Title"          },
      { "author",       "Video.Author"         },
      { "copyright",    "File.Copyright"      },
      { NULL,           NULL                   }
   };

void
tracker_extract_mplayer (gchar *filename, GHashTable *metadata)
{
	gchar      	*argv[10];
	gchar      	*mplayer;
	gchar      	**lines, **line;
   	gint        	i;

	argv[0] = g_strdup ("mplayer");
	argv[1] = g_strdup ("-identify");
	argv[2] = g_strdup ("-frames");
	argv[3] = g_strdup ("0");
	argv[4] = g_strdup ("-vo");
	argv[5] = g_strdup ("null");
	argv[6] = g_strdup ("-ao");
	argv[7] = g_strdup ("null");
	argv[8] = g_strdup (filename);
	argv[9] = NULL;

	if(g_spawn_sync (NULL,
	                 argv,
	                 NULL,
	                 G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
	                 NULL,
	                 NULL,
	                 &mplayer,
	                 NULL,
	                 NULL,
	                 NULL)) {


         	lines = g_strsplit (mplayer, "\n", -1);

		for (line = lines; *line; ++line) {

         		if (g_pattern_match_simple ("ID_VIDEO*=*", *line)) {
               			for (i=0; video_tags[i][0]; i++) {
                  			if (g_str_has_prefix (*line, video_tags[i][0])) {
                     				g_hash_table_insert (metadata, 
	                           		g_strdup (video_tags[i][1]), 
        	                   		g_strdup ((*line) + strlen (video_tags[i][0]) + 1));
                	  		}
               			}
	            	}

        	    	if (g_pattern_match_simple ("ID_AUDIO*=*", *line)) {
               			for (i=0; audio_tags[i][0]; i++) {
               				if (g_str_has_prefix (*line, audio_tags[i][0])) {
                     				g_hash_table_insert (metadata, 
                           			g_strdup (audio_tags[i][1]), 
	                           		g_strdup ((*line) + strlen (audio_tags[i][0]) + 1));
        	          		}
               			}
            		}

	            	if (g_pattern_match_simple ("ID_CLIP_INFO_NAME*=*", *line)) {
        	       		for (i=0; info_tags[i][0]; i++) {
               				if (g_str_has_prefix (*line, info_tags[i][0])) {
                     				g_hash_table_insert (metadata, 
                           			g_strdup (info_tags[i][1]), 
                           			g_strdup ((*line) + strlen (info_tags[i][0]) + 1));
	                  		}
        	       		}
            		}
		}
	}
}
