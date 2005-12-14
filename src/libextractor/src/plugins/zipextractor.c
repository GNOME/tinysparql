/** 
     zipextractor.c  version 0.0.2

     Changes from 0.0.1 to 0.0.2 
     -> Searches for central dir struct from end of file if this is a self-extracting executable


     This file was based on mp3extractor.c  (0.1.2)

     Currently, this only returns a list of the filenames within a zipfile
     and any comments on each file or the whole file itself. File sizes, 
     modification times, and crc's are currently ignored.

     TODO: Break the comments up into small, atomically, searchable chunks (keywords)
         - might need some knowledge of English?

     It returns:

     one      EXTRACTOR_MIMETYPE
     multiple EXTRACTOR_FILENAME
     multiple EXTRACTOR_COMMENT
     
     ... from a .ZIP file

     TODO: EXTRACTOR_DATE, EXTRACTOR_DESCRIPTION, EXTRACTOR_KEYWORDS, others?

     Does NOT test data integrity (CRCs etc.)

     This version is not recursive (i.e. doesn't look inside zip 
     files within zip files)
     
     TODO: Run extract on files inside of archive (?) (i.e. gif, mp3, etc.)
     
     The current .ZIP format description:
     ftp://ftp.pkware.com/appnote.zip

     No Copyright 2003 Julia Wolf

 */
 
/*
 * This file is part of libextractor.
 * (C) 2002, 2003 Vidyut Samanta and Christian Grothoff
 *
 * libextractor is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 2, or (at your
 * option) any later version.
 * 
 * libextractor is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libextractor; see the file COPYING.  If not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "platform.h"
#include "extractor.h"

#define DEBUG_EXTRACT_ZIP 0


/* In a zipfile there are two kinds of comments. One is a big one for the
   entire .zip, it's usually a BBS ad. The other is a small comment on each
   individual file; most people don't use this.
 */
  
/* TODO: zip_entry linked list is handeled kinda messily, should clean up (maybe) */

typedef struct {
  char *filename;
  char *comment;
  void *next;
} zip_entry;



/* mimetype = application/zip */
struct EXTRACTOR_Keywords * 
libextractor_zip_extract(const char * filename,
			 const unsigned char * data,
			 size_t size,
			 struct EXTRACTOR_Keywords * prev) {
  void * tmp;
  zip_entry * info;
  zip_entry * start;
  char *filecomment = NULL;
  const unsigned char * pos;
  unsigned int offset, stop;
  unsigned int name_length, extra_length, comment_length;
  unsigned int filecomment_length;
  unsigned int entry_total, entry_count; 
  EXTRACTOR_KeywordList * keyword;
  const char * mimetype;

  mimetype = EXTRACTOR_extractLast(EXTRACTOR_MIMETYPE, 
				   prev);
  if (NULL != mimetype) {
    if ( (0 != strcmp(mimetype, "application/x-zip")) &&
	 (0 != strcmp(mimetype, "application/zip")) ) {
      /* we think we already know what's in here,
	 and it is not a zip */
      return prev;
    }
  }

  /* I think the smallest zipfile you can have is about 120 bytes */
  if ( (NULL==data) || (size < 100) )
    return prev;  

  if ( !( ('P'==data[0]) && ('K'==data[1]) && (0x03==data[2]) && (0x04==data[3])) )
    return prev;
  
  /* The filenames for each file in a zipfile are stored in two locations.
   * There is one at the start of each entry, just before the compressed data,
   * and another at the end in a 'central directory structure'.
   *
   * In order to catch self-extracting executables, we scan backwards from the end
   * of the file looking for the central directory structure. The previous version
   * of this went forewards through the local headers, but that only works for plain
   * vanilla zip's and I don't feel like writing a special case for each of the dozen
   * self-extracting executable stubs.
   *
   * This assumes that the zip file is considered to be non-corrupt/non-truncated.
   * If it is truncated then it's not considered to be a zip and skipped.
   *
   */

  /* From appnote.iz and appnote.txt (more or less)
   *
   *   (this is why you always need to put in the last floppy if you span disks)
   *
   *   0- 3  end of central dir signature    4 bytes  (0x06054b50) P K ^E ^F
   *   4- 5  number of this disk             2 bytes
   *   6- 7  number of the disk with the
   *         start of the central directory  2 bytes
   *   8- 9  total number of entries in
   *         the central dir on this disk    2 bytes
   *  10-11  total number of entries in
   *         the central dir                 2 bytes
   *  12-15  size of the central directory   4 bytes
   *  16-19  offset of start of central
   *         directory with respect to
   *         the starting disk number        4 bytes
   *  20-21  zipfile comment length          2 bytes
   *  22-??  zipfile comment (variable size) max length 65536 bytes
   */


  /*  the signature can't be more than 22 bytes from the end */
  offset = size-22;
  pos = &data[offset];

  stop = 0;
  if ( ((signed int)size-65556) > 0) 
    stop = size-65556;

  /* not using int 0x06054b50 so that we don't have to deal with endianess issues.
     break out if we go more than 64K backwards and havn't found it, or if we hit the
     begining of the file. */

  while ( ( !( ('P'==pos[0]) && ('K'==pos[1]) && (0x05==pos[2]) && (0x06==pos[3])) ) &&
	  (offset > stop) )
    pos = &data[offset--];
  
  if (offset==stop) {
#if DEBUG_EXTRACT_ZIP
    fprintf(stderr,
	    "Did not find end of central directory structure signature. offset: %i\n",
	    offset);
#endif
    return prev;
  }

  /* offset should now point to the start of the end-of-central directory structure */
  /* and pos[0] should be pointing there too */
  /* so slurp down filecomment while here... */

  filecomment_length = pos[20] + (pos[21]<<8);
  if (filecomment_length + offset + 22 > size) {
    return prev; /* invalid zip file format! */
  }
  filecomment = NULL;
  if (filecomment_length > 0) {
    filecomment = malloc(filecomment_length+1);
    memcpy(filecomment,
	   &pos[22], 
	   filecomment_length);
    filecomment[filecomment_length] = '\0';
  }


  if ( (0!=pos[4])&&(0!=pos[5]) ) {
#if DEBUG_EXTRACT_ZIP
    fprintf(stderr,
	    "WARNING: This seems to be the last disk in a multi-volume"
	    " ZIP archive, and so this might not work.\n");
#endif
  }
  if ( (pos[8]!=pos[10])&&(pos[9]!=pos[11]) ) {
#if DEBUG_EXTRACT_ZIP
     fprintf(stderr,
	     "WARNING: May not be able to find all the files in this"
	     " ZIP archive (no multi-volume support right now).\n");
#endif
  }

  entry_total = pos[10]+(pos[11]<<8);
  entry_count = 0;

  /* jump to start of central directory, ASSUMING that the starting disk that it's on is disk 0 */
  /* starting disk would otherwise be pos[6]+pos[7]<<8 */

  offset = pos[16] + (pos[17]<<8) + (pos[18]<<16) + (pos[19]<<24); /* offset of cent-dir from start of disk 0 */
  /* stop   = pos[12] + (pos[13]<<8) + (pos[14]<<16) + (pos[15]<<24); */ /* length of central dir */
  if (offset + 46 > size) {
    /* not a zip */
    if (filecomment != NULL)
      free(filecomment);
    return prev;    
  }
  pos = &data[offset]; /* jump */

  /* we should now be at the begining of the central directory structure */

  /* from appnote.txt and appnote.iz (mostly)
   *
   *   0- 3  central file header signature   4 bytes  (0x02014b50)
   *   4- 5  version made by                 2 bytes
   *   6- 7  version needed to extract       2 bytes
   *   8- 9  general purpose bit flag        2 bytes
   *  10-11  compression method              2 bytes
   *  12-13  last mod file time              2 bytes
   *  14-15  last mod file date              2 bytes
   *  16-19  crc-32                          4 bytes
   *  20-23  compressed size                 4 bytes
   *  24-27  uncompressed size               4 bytes
   *  28-29  filename length                 2 bytes
   *  30-31  extra field length              2 bytes
   *  32-33  file comment length             2 bytes
   *  34-35  disk number start               2 bytes
   *  36-37  internal file attributes        2 bytes
   *  38-41  external file attributes        4 bytes
   *  42-45  relative offset of local header 4 bytes
   *
   *  46-??  filename (variable size)
   *   ?- ?  extra field (variable size)
   *   ?- ?  file comment (variable size)
   */

  if ( !(('P'==pos[0])&&('K'==pos[1])&&(0x01==pos[2])&&(0x02==pos[3])) ) {
#if DEBUG_EXTRACT_ZIP
    fprintf(stderr,
	    "Did not find central directory structure signature. offset: %i\n",
	    offset);
#endif
    if (filecomment != NULL)
      free(filecomment);
    return prev;
  }

  start = NULL;
  info = NULL;
  do {   /* while ( (0x01==pos[2])&&(0x02==pos[3]) ) */
    entry_count++; /* check to make sure we found everything at the end */

    name_length     = pos[28] + (pos[29]<<8);
    extra_length    = pos[30] + (pos[31]<<8);
    comment_length  = pos[32] + (pos[33]<<8);

    if (name_length + extra_length + comment_length + offset + 46 > size) {
      /* ok, invalid, abort! */
      break;
    }

#if DEBUG_EXTRACT_ZIP
    fprintf(stderr,
	    "Found filename length %i  Comment length: %i\n", 
	    name_length, 
	    comment_length);
#endif

    /* yay, finally get filenames */
    if (start == NULL) {
      start = malloc(sizeof(zip_entry));
      start->next = NULL;
      info = start;
    } else {
      info->next = malloc(sizeof(zip_entry));
      info = info->next;
      info->next = NULL;
    }
    info->filename = malloc(name_length + 1);
    info->comment  = malloc(comment_length + 1);

    /* (strings in zip files are not null terminated) */
    memcpy(info->filename, 
	   &pos[46],
	   name_length); 
    info->filename[name_length] = '\0';
    memcpy(info->comment, 
	   &pos[46+name_length+extra_length], 
	   comment_length);
    info->comment[comment_length] = '\0';

#if DEBUG_EXTRACT_ZIP
    fprintf(stderr,
	    "Found file %s, Comment: %s\n", 
	    info->filename,
	    info->comment);
#endif

    offset += 46 + name_length + extra_length + comment_length;
    pos = &data[offset];
  
    /* check for next header entry (0x02014b50) or (0x06054b50) if at end */
    if ( ('P'!=pos[0])&&('K'!=pos[1]) ) {
#if DEBUG_EXTRACT_ZIP
      fprintf(stderr,
	      "Did not find next header in central directory.\n");
#endif
      info = start;
      while (info != NULL) {
	start = info->next;
	free(info->filename);
	free(info->comment);
	free(info);	
	info = start;
      }
      if (filecomment != NULL)
	free(filecomment);
      return prev;
    }

  } while ( (0x01==pos[2])&&(0x02==pos[3]) );

  /* end list */


  /* TODO: should this return an error? indicates corrupt zipfile (or
     disk missing in middle of multi-disk)? */
  if (entry_count != entry_total) {
#if DEBUG_EXTRACT_ZIP
     fprintf(stderr,
	     "WARNING: Did not find all of the zipfile entries that we should have.\n");
#endif
  }

  /* I'm only putting this in the else clause so that keyword has a local scope */
  keyword
    = malloc(sizeof(EXTRACTOR_KeywordList));    
  keyword->next = prev;
  keyword->keyword = strdup("application/zip");
  keyword->keywordType = EXTRACTOR_MIMETYPE;
  prev = keyword;   

  if (filecomment != NULL) {
    EXTRACTOR_KeywordList * kw
      = malloc(sizeof(EXTRACTOR_KeywordList));
    kw->next = prev;    
    kw->keyword = strdup(filecomment); 
    kw->keywordType = EXTRACTOR_COMMENT;
    prev = kw;
    free(filecomment);
  }

  /* if we've gotten to here then there is at least one zip entry (see get_zipinfo call above) */
  /* note: this free()'s the info list as it goes */
  info = start;
  while (NULL != info) {
    if (strlen(info->filename)){
      EXTRACTOR_KeywordList * keyword = malloc(sizeof(EXTRACTOR_KeywordList));
      keyword->next = prev;    
      keyword->keyword = strdup(info->filename);
      keyword->keywordType = EXTRACTOR_FILENAME;
      prev = keyword;
    }  
    if (info->filename != NULL)
      free(info->filename);

    if (strlen(info->comment)){
      EXTRACTOR_KeywordList * keyword = malloc(sizeof(EXTRACTOR_KeywordList));    
      keyword->next = prev;
      keyword->keyword = strdup(info->comment);
      keyword->keywordType = EXTRACTOR_COMMENT;
      prev = keyword;
    }
    if (info->comment != NULL)
      free(info->comment);
    tmp = info;
    info = info->next;
    free(tmp);
  }
  return prev;
}
