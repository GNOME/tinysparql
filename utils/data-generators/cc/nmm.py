# -*- coding: utf-8 -*-

import tools

####################################################################################
def generatePhoto(index):
  me = 'nmm#Photo'
  photo_uri          = 'urn:photo:%d' % index
  photo_filename     = 'photo%d.jpg' % index
  photo_url          = 'file:///path/' + photo_filename
  photo_size         = str(1000000 + index)
  if (index % 10 == 0):
    photo_camera     = "NOKIA"
  else:
    photo_camera     = 'Canikon 1000 Ultra Mega'
  photo_exposure     = '0.%d' % index
  photo_fnumber      = '%d.0' % (1 + (index % 20))
  photo_focal_length = '%d.0' % (1 + (index % 500))
  photo_date         = '%d-%02d-%02dT01:01:01Z' % (2000 + (index % 10), (index % 12) + 1, (index % 25) + 1)

  # save the last uri
  tools.addUri( me, photo_uri )

  # subsitute into template
  photo = tools.getTemplate( me )

  # save the result
  tools.addResult( me, photo % locals() )

####################################################################################
def generateVideo(index):
  me = 'nmm#Video'
  video_uri          = 'urn:video:%d' % index
  video_filename     = 'video%d.jpg' % index
  video_url          = 'file:///path/' + video_filename
  video_size         = str(1000000 + index)
  video_date         = '%d-%02d-%02dT01:01:01Z' % (2000 + (index % 10), (index % 12) + 1, (index % 25) + 1)
  video_samplerate   = str(index)
  video_duration     = str(index)
  video_tag          = 'TEST%d' % index

  # save the last uri
  tools.addUri( me, video_uri )

  # subsitute into template
  video = tools.getTemplate( me )

  # save the result
  tools.addResult( me, video % locals() )

####################################################################################
def generateArtist(index):
  me = 'nmm#Artist'
  artist_uri  = 'urn:artist:%d' % index
  artist_name = 'Artist %d' % (index % 1000)

  # save the last uri
  tools.addUri( me, artist_uri )

  # subsitute into template
  artist = tools.getTemplate( me )

  # save the result
  tools.addResult( me, artist % locals() )

####################################################################################
def generateAlbum(index):
  me = 'nmm#MusicAlbum'
  album_uri  = 'urn:album:%d' % index
  album_name = 'Album %d' % (index % 1000)

  # save the last uri
  tools.addUri( me, album_uri )

  # subsitute into template
  album = tools.getTemplate( me )

  # save the result
  tools.addResult( me, album % locals() )

####################################################################################
def generateMusicPiece(index):
  me = 'nmm#MusicPiece'
  music_piece_uri           = 'urn:music:%d' % index
  music_piece_title         = 'Song %d' % (index % 1000)
  music_piece_size          = str(index)
  music_piece_container     = 'file://music/'
  music_piece_filename      = 'Song_%d.mp3' % (index % 1000)
  music_piece_url           = music_piece_container + music_piece_filename
  music_piece_mime          = 'audio/mpeg'
  music_piece_date          = '%d-%02d-%02dT01:01:01Z' % (2000 + (index % 10), (index % 12) + 1, (index % 25) + 1)
  music_piece_last_accessed = music_piece_date
  music_piece_last_modified = music_piece_date
  music_piece_codec         = 'MPEG'
  music_piece_bitrate       = '%d' % (16 + (index % 300))
  music_piece_genre         = 'Genre %d' % (index % 1000)
  music_piece_channels      = '2'
  music_piece_sample_rate   = '44100.0'
  music_piece_album         = tools.getRandomUri( 'nmm#MusicAlbum' )
  music_piece_artist        = tools.getRandomUri( 'nmm#Artist' )
  music_piece_length        = '%d' % (1 + (index % 1000))
  music_piece_track         = '%d' % (1 + (index % 100))

  # save the last uri
  tools.addUri( me, music_piece_uri )

  # subsitute into template
  mp = tools.getTemplate( me )

  # save the result
  tools.addResult( me, mp % locals() )
