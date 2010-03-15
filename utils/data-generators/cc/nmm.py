# -*- coding: utf-8 -*-

import tools

####################################################################################
def generatePhoto(index):
  photo_uri          = 'urn:photo:%d' % index
  photo_filename     = 'photo%d.jpg' % index
  photo_url          = 'file:///path/' + photo_filename
  photo_size         = str(1000000 + index)
  photo_camera       = 'Canikon 1000 Ultra Mega'
  photo_exposure     = '0.%d' % index
  photo_fnumber      = '%d.0' % (1 + (index % 20))
  photo_focal_length = '%d.0' % (1 + (index % 500))
  photo_date         = '%d-%02d-%02dT01:01:01Z' % (2000 + (index % 10), (index % 12) + 1, (index % 25) + 1)

  # save the last uri
  tools.addUri( 'nmm#Photo', photo_uri )

  # subsitute into template
  photo = tools.getTemplate( 'nmm#Photo' )
  photo = photo.replace( '${photo_uri}', photo_uri )
  photo = photo.replace( '${photo_filename}', photo_filename )
  photo = photo.replace( '${photo_url}', photo_url )
  photo = photo.replace( '${photo_size}',  photo_size)
  photo = photo.replace( '${photo_camera}', photo_camera )
  photo = photo.replace( '${photo_exposure}', photo_exposure )
  photo = photo.replace( '${photo_fnumber}', photo_fnumber )
  photo = photo.replace( '${photo_focal_length}', photo_focal_length )
  photo = photo.replace( '${photo_date}', photo_date )

  # save the result
  tools.addResult( 'nmm#Photo', photo )

####################################################################################
def generateArtist(index):
  artist_uri  = 'urn:artist:%d' % index
  artist_name = 'Artist %d' % (index % 1000)

  # save the last uri
  tools.addUri( 'nmm#Artist', artist_uri )

  # subsitute into template
  artist = tools.getTemplate( 'nmm#Artist' )
  artist = artist.replace( '${artist_uri}', artist_uri )
  artist = artist.replace( '${artist_name}', artist_name )

  # save the result
  tools.addResult( 'nmm#Artist', artist )

####################################################################################
def generateAlbum(index):
  album_uri  = 'urn:album:%d' % index
  album_name = 'Album %d' % (index % 1000)

  # save the last uri
  tools.addUri( 'nmm#MusicAlbum', album_uri )

  # subsitute into template
  album = tools.getTemplate( 'nmm#MusicAlbum' )
  album = album.replace( '${album_uri}', album_uri )
  album = album.replace( '${album_name}', album_name )

  # save the result
  tools.addResult( 'nmm#MusicAlbum', album )

####################################################################################
def generateMusicPiece(index):
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
  tools.addUri( 'nmm#MusicPiece', music_piece_uri )

  # subsitute into template
  mp = tools.getTemplate( 'nmm#MusicPiece' )
  mp = mp.replace( '${music_piece_uri}', music_piece_uri )
  mp = mp.replace( '${music_piece_title}', music_piece_title )
  mp = mp.replace( '${music_piece_size}', music_piece_size )
  mp = mp.replace( '${music_piece_container}', music_piece_container )
  mp = mp.replace( '${music_piece_filename}', music_piece_filename )
  mp = mp.replace( '${music_piece_url}', music_piece_url )
  mp = mp.replace( '${music_piece_mime}', music_piece_mime )
  mp = mp.replace( '${music_piece_date}', music_piece_date )
  mp = mp.replace( '${music_piece_date}', music_piece_date )
  mp = mp.replace( '${music_piece_last_accessed}', music_piece_last_accessed )
  mp = mp.replace( '${music_piece_last_modified}', music_piece_last_modified )
  mp = mp.replace( '${music_piece_codec}', music_piece_codec )
  mp = mp.replace( '${music_piece_bitrate}', music_piece_bitrate )
  mp = mp.replace( '${music_piece_genre}', music_piece_genre )
  mp = mp.replace( '${music_piece_channels}', music_piece_channels )
  mp = mp.replace( '${music_piece_sample_rate}', music_piece_sample_rate )
  mp = mp.replace( '${music_piece_album}', music_piece_album )
  mp = mp.replace( '${music_piece_artist}', music_piece_artist )
  mp = mp.replace( '${music_piece_length}', music_piece_length )
  mp = mp.replace( '${music_piece_track}', music_piece_track )

  # save the result
  tools.addResult( 'nmm#MusicPiece', mp )
