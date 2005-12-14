/*
     This file is part of libextractor.
     (C) 2004,2005 Vidyut Samanta and Christian Grothoff

     libextractor is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License as published
     by the Free Software Foundation; either version 2, or (at your
     option) any later version.

     libextractor is distributed in the hope that it will be useful, but
     WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
     General Public License for more details.

     You should have received a copy of the GNU General Public License
     along with libextractor; see the file COPYING.  If not, write to the
     Free Software Foundation, Inc., 59 Temple Place - Suite 330,
     Boston, MA 02111-1307, USA.
 */

#include "platform.h"
#include "extractor.h"
#include <glib-object.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#define DEBUG_OLE2 0

#if DEBUG_OLE2
#define d(code)	do { code } while (0)
#define warning printf
#else
#define d(code)
 static void warning(const char * format, ...) {}
#endif

#undef g_return_val_if_fail
#define g_return_val_if_fail(a,b) if (! (a)) return (b);
 
/* *********************** formerly gsf-input.c ************* */

typedef struct GsfInput {
	off_t size;
	off_t cur_offset;
	char * name;
	const unsigned char * buf;
	int needs_free;
} GsfInput;


static void
gsf_input_init (GsfInput * input)
{
	input->size = 0;
	input->cur_offset = 0;
	input->name = NULL;
	input->buf = NULL;
}

/**
 * gsf_input_memory_new:
 * @buf: The input bytes
 * @length: The length of @buf
 * @needs_free: Whether you want this memory to be free'd at object destruction
 *
 * Returns: A new #GsfInputMemory
 */
static GsfInput *
gsf_input_new (const unsigned char * buf,
	       off_t length,
	       int needs_free)
{
	GsfInput *mem = malloc(sizeof(GsfInput));
	if (mem == NULL)
		return NULL;
	gsf_input_init(mem);
	mem->buf = buf;
	mem->size = length;
	mem->needs_free = needs_free;
	return mem;
}

static void
gsf_input_finalize (GsfInput * input)
{
	if (input->name != NULL) {
		free (input->name);
		input->name = NULL;
	}
	if ( (input->buf) && input->needs_free)
		free((void*) input->buf);
	free(input);
}

/**
 * gsf_input_set_name :
 * @input :
 * @name :
 *
 * protected.
 *
 * Returns : TRUE if the assignment was ok.
 **/
static int
gsf_input_set_name (GsfInput *input, char const *name)
{
	char *buf;

	g_return_val_if_fail (input != NULL, 0);

	buf = strdup (name);
	if (input->name != NULL)
		free (input->name);
	input->name = buf;
	return 1;
}



static GsfInput *
gsf_input_dup (GsfInput *src)
{
	GsfInput * dst = malloc(sizeof(GsfInput));
	if (dst == NULL)
		return NULL;
        gsf_input_init(dst);
	dst->buf = src->buf;
	dst->needs_free = 0;
	dst->size = src->size;
	if (src->name != NULL)
		gsf_input_set_name (dst, src->name);
	dst->cur_offset = src->cur_offset;
	return dst;
}

static const unsigned char *
gsf_input_read (GsfInput * mem, size_t num_bytes, unsigned char * optional_buffer)
{
	const unsigned char *src = mem->buf;
	if (src == NULL)
		return NULL;
	if (optional_buffer) {
		memcpy (optional_buffer, src + mem->cur_offset, num_bytes);
		mem->cur_offset += num_bytes;

		return optional_buffer;
	} else {
		const unsigned char * ret = src + mem->cur_offset;
		mem->cur_offset += num_bytes;
		return ret;
	}
}

/**
 * gsf_input_size :
 * @input : The input
 *
 * Looks up and caches the number of bytes in the input
 *
 * Returns :  the size or -1 on error
 **/
static off_t
gsf_input_size (GsfInput *input)
{
	g_return_val_if_fail (input != NULL, -1);
	return input->size;
}

/**
 * gsf_input_seek :
 * @input :
 * @offset :
 * @whence :
 *
 * Returns TRUE on error.
 **/
static int
gsf_input_seek (GsfInput *input, off_t offset, int whence)
{
	off_t pos = offset;

	g_return_val_if_fail (input != NULL, 1);

	switch (whence) {
	case SEEK_SET : break;
	case SEEK_CUR : pos += input->cur_offset;	break;
	case SEEK_END : pos += input->size;		break;
	default : return 1;
	}

	if (pos < 0 || pos > input->size)
		return 1;

	/*
	 * If we go nowhere, just return.  This in particular handles null
	 * seeks for streams with no seek method.
	 */
	if (pos == input->cur_offset)
		return 0;

	input->cur_offset = pos;
	return 0;
}




/* ******************** formerly gsf-utils.c **************** */


/* Do this the ugly way so that we don't have to worry about alignment */
#define GSF_LE_GET_GUINT8(p) (*(guint8 const *)(p))
#define GSF_LE_GET_GUINT16(p)				\
	(guint16)((((guint8 const *)(p))[0] << 0)  |	\
		  (((guint8 const *)(p))[1] << 8))
#define GSF_LE_GET_GUINT32(p)				\
	(guint32)((((guint8 const *)(p))[0] << 0)  |	\
		  (((guint8 const *)(p))[1] << 8)  |	\
		  (((guint8 const *)(p))[2] << 16) |	\
		  (((guint8 const *)(p))[3] << 24))

#define GSF_LE_GET_GUINT64(p) (gsf_le_get_guint64 (p))
#define GSF_LE_GET_GINT64(p) ((gint64)GSF_LE_GET_GUINT64(p))
#define GSF_LE_GET_GINT8(p) ((gint8)GSF_LE_GET_GUINT8(p))
#define GSF_LE_GET_GINT16(p) ((gint16)GSF_LE_GET_GUINT16(p))
#define GSF_LE_GET_GINT32(p) ((gint32)GSF_LE_GET_GUINT32(p))
#define GSF_LE_GET_FLOAT(p) (gsf_le_get_float (p))
#define GSF_LE_GET_DOUBLE(p) (gsf_le_get_double (p))
#define GSF_LE_SET_GUINT8(p, dat)			\
	(*((guint8 *)(p))      = ((dat)        & 0xff))
#define GSF_LE_SET_GUINT16(p, dat)			\
	((*((guint8 *)(p) + 0) = ((dat)        & 0xff)),\
	 (*((guint8 *)(p) + 1) = ((dat) >>  8) & 0xff))
#define GSF_LE_SET_GUINT32(p, dat)				\
	((*((guint8 *)(p) + 0) = ((dat))       & 0xff),	\
	 (*((guint8 *)(p) + 1) = ((dat) >>  8) & 0xff),	\
	 (*((guint8 *)(p) + 2) = ((dat) >> 16) & 0xff),	\
	 (*((guint8 *)(p) + 3) = ((dat) >> 24) & 0xff))
#define GSF_LE_SET_GINT8(p,dat) GSF_LE_SET_GUINT8((p),(dat))
#define GSF_LE_SET_GINT16(p,dat) GSF_LE_SET_GUINT16((p),(dat))
#define GSF_LE_SET_GINT32(p,dat) GSF_LE_SET_GUINT32((p),(dat))


/*
 * Glib gets this wrong, really.  ARM's floating point format is a weird
 * mixture.
 */
#define G_ARMFLOAT_ENDIAN 56781234
#if defined(__arm__) && !defined(__vfp__) && (G_BYTE_ORDER == G_LITTLE_ENDIAN)
#define G_FLOAT_BYTE_ORDER G_ARMFLOAT_ENDIAN
#else
#define G_FLOAT_BYTE_ORDER G_BYTE_ORDER
#endif

static guint64
gsf_le_get_guint64 (void const *p)
{
#if G_BYTE_ORDER == G_BIG_ENDIAN
	if (sizeof (guint64) == 8) {
		guint64 li;
		int     i;
		guint8 *t  = (guint8 *)&li;
		guint8 *p2 = (guint8 *)p;
		int     sd = sizeof (li);

		for (i = 0; i < sd; i++)
			t[i] = p2[sd - 1 - i];

		return li;
	} else {
		g_error ("Big endian machine, but weird size of guint64");
	}
#elif G_BYTE_ORDER == G_LITTLE_ENDIAN
	if (sizeof (guint64) == 8) {
		/*
		 * On i86, we could access directly, but Alphas require
		 * aligned access.
		 */
		guint64 data;
		memcpy (&data, p, sizeof (data));
		return data;
	} else {
		g_error ("Little endian machine, but weird size of guint64");
	}
#else
#error "Byte order not recognised -- out of luck"
#endif
}

static float
gsf_le_get_float (void const *p)
{
#if G_FLOAT_BYTE_ORDER == G_BIG_ENDIAN
	if (sizeof (float) == 4) {
		float   f;
		int     i;
		guint8 *t  = (guint8 *)&f;
		guint8 *p2 = (guint8 *)p;
		int     sd = sizeof (f);

		for (i = 0; i < sd; i++)
			t[i] = p2[sd - 1 - i];

		return f;
	} else {
		g_error ("Big endian machine, but weird size of floats");
	}
#elif (G_FLOAT_BYTE_ORDER == G_LITTLE_ENDIAN) || (G_FLOAT_BYTE_ORDER == G_ARMFLOAT_ENDIAN)
	if (sizeof (float) == 4) {
		/*
		 * On i86, we could access directly, but Alphas require
		 * aligned access.
		 */
		float data;
		memcpy (&data, p, sizeof (data));
		return data;
	} else {
		g_error ("Little endian machine, but weird size of floats");
	}
#else
#error "Floating-point byte order not recognised -- out of luck"
#endif
}

static double
gsf_le_get_double (void const *p)
{
#if G_FLOAT_BYTE_ORDER == G_ARMFLOAT_ENDIAN
	double data;
	memcpy ((char *)&data + 4, p, 4);
	memcpy ((char *)&data, (const char *)p + 4, 4);
	return data;
#elif G_FLOAT_BYTE_ORDER == G_BIG_ENDIAN
	if (sizeof (double) == 8) {
		double  d;
		int     i;
		guint8 *t  = (guint8 *)&d;
		guint8 *p2 = (guint8 *)p;
		int     sd = sizeof (d);

		for (i = 0; i < sd; i++)
			t[i] = p2[sd - 1 - i];

		return d;
	} else {
		g_error ("Big endian machine, but weird size of doubles");
	}
#elif G_FLOAT_BYTE_ORDER == G_LITTLE_ENDIAN
	if (sizeof (double) == 8) {
		/*
		 * On i86, we could access directly, but Alphas require
		 * aligned access.
		 */
		double data;
		memcpy (&data, p, sizeof (data));
		return data;
	} else {
		g_error ("Little endian machine, but weird size of doubles");
	}
#else
#error "Floating-point byte order not recognised -- out of luck"
#endif
}

/**
 * gsf_iconv_close : A utility wrapper to safely close an iconv handle
 * @handle :
 **/
static void
gsf_iconv_close (GIConv handle)
{
	if (handle != NULL && handle != ((GIConv)-1))
		g_iconv_close (handle);
}


/* ***************************** formerly gsf-infile-msole.c ********************* */

#define OLE_HEADER_SIZE		 0x200	/* independent of big block size size */
#define OLE_HEADER_SIGNATURE	 0x00
#define OLE_HEADER_CLSID	 0x08	/* See ReadClassStg */
#define OLE_HEADER_MINOR_VER	 0x18	/* 0x33 and 0x3e have been seen */
#define OLE_HEADER_MAJOR_VER	 0x1a	/* 0x3 been seen in wild */
#define OLE_HEADER_BYTE_ORDER	 0x1c	/* 0xfe 0xff == Intel Little Endian */
#define OLE_HEADER_BB_SHIFT      0x1e
#define OLE_HEADER_SB_SHIFT      0x20
/* 0x22..0x27 reserved == 0 */
#define OLE_HEADER_CSECTDIR	 0x28
#define OLE_HEADER_NUM_BAT	 0x2c
#define OLE_HEADER_DIRENT_START  0x30
/* 0x34..0x37 transacting signature must be 0 */
#define OLE_HEADER_THRESHOLD	 0x38
#define OLE_HEADER_SBAT_START    0x3c
#define OLE_HEADER_NUM_SBAT      0x40
#define OLE_HEADER_METABAT_BLOCK 0x44
#define OLE_HEADER_NUM_METABAT   0x48
#define OLE_HEADER_START_BAT	 0x4c
#define BAT_INDEX_SIZE		 4
#define OLE_HEADER_METABAT_SIZE	 ((OLE_HEADER_SIZE - OLE_HEADER_START_BAT) / BAT_INDEX_SIZE)

#define DIRENT_MAX_NAME_SIZE	0x40
#define DIRENT_DETAILS_SIZE	0x40
#define DIRENT_SIZE		(DIRENT_MAX_NAME_SIZE + DIRENT_DETAILS_SIZE)
#define DIRENT_NAME_LEN		0x40	/* length in bytes incl 0 terminator */
#define DIRENT_TYPE		0x42
#define DIRENT_COLOUR		0x43
#define DIRENT_PREV		0x44
#define DIRENT_NEXT		0x48
#define DIRENT_CHILD		0x4c
#define DIRENT_CLSID		0x50	/* only for dirs */
#define DIRENT_USERFLAGS	0x60	/* only for dirs */
#define DIRENT_CREATE_TIME	0x64	/* for files */
#define DIRENT_MODIFY_TIME	0x6c	/* for files */
#define DIRENT_FIRSTBLOCK	0x74
#define DIRENT_FILE_SIZE	0x78
/* 0x7c..0x7f reserved == 0 */

#define DIRENT_TYPE_INVALID	0
#define DIRENT_TYPE_DIR		1
#define DIRENT_TYPE_FILE	2
#define DIRENT_TYPE_LOCKBYTES	3	/* ? */
#define DIRENT_TYPE_PROPERTY	4	/* ? */
#define DIRENT_TYPE_ROOTDIR	5
#define DIRENT_MAGIC_END	0xffffffff

/* flags in the block allocation list to denote special blocks */
#define BAT_MAGIC_UNUSED	0xffffffff	/*		   -1 */
#define BAT_MAGIC_END_OF_CHAIN	0xfffffffe	/*		   -2 */
#define BAT_MAGIC_BAT		0xfffffffd	/* a bat block,    -3 */
#define BAT_MAGIC_METABAT	0xfffffffc	/* a metabat block -4 */




typedef struct {
	guint32 *block;
	guint32  num_blocks;
} MSOleBAT;

typedef struct {
	char	 *name;
	char	 *collation_name;
	int	  index;
	size_t    size;
	gboolean  use_sb;
	guint32   first_block;
	gboolean  is_directory;
	GList	 *children;
	unsigned char clsid[16];	/* 16 byte GUID used by some apps */
} MSOleDirent;

typedef struct {
	struct {
		MSOleBAT bat;
		unsigned shift;
		unsigned filter;
		size_t   size;
	} bb, sb;
	off_t max_block;
	guint32 threshold; /* transition between small and big blocks */
        guint32 sbat_start, num_sbat;

	MSOleDirent *root_dir;
	struct GsfInput *sb_file;

	int ref_count;
} MSOleInfo;

typedef struct GsfInfileMSOle {
	off_t size;
	off_t cur_offset;
	struct GsfInput    *input;
	MSOleInfo   *info;
	MSOleDirent *dirent;
	MSOleBAT     bat;
	off_t    cur_block;

	struct {
		guint8  *buf;
		size_t  buf_size;
	} stream;
} GsfInfileMSOle;

/* utility macros */
#define OLE_BIG_BLOCK(index, ole)	((index) >> ole->info->bb.shift)

static struct GsfInput *gsf_infile_msole_new_child (GsfInfileMSOle *parent,
					     MSOleDirent *dirent);

/**
 * ole_get_block :
 * @ole    : the infile
 * @block  :
 * @buffer : optionally NULL
 *
 * Read a block of data from the underlying input.
 * Be really anal.
 **/
static const guint8 *
ole_get_block (const GsfInfileMSOle *ole, guint32 block, guint8 *buffer)
{
	g_return_val_if_fail (block < ole->info->max_block, NULL);

	/* OLE_HEADER_SIZE is fixed at 512, but the sector containing the
	 * header is padded out to bb.size (sector size) when bb.size > 512. */
	if (gsf_input_seek (ole->input,
		(off_t)(MAX (OLE_HEADER_SIZE, ole->info->bb.size) + (block << ole->info->bb.shift)),
		SEEK_SET) < 0)
		return NULL;

	return gsf_input_read (ole->input, ole->info->bb.size, buffer);
}

/**
 * ole_make_bat :
 * @metabat	: a meta bat to connect to the raw blocks (small or large)
 * @size_guess	: An optional guess as to how many blocks are in the file
 * @block	: The first block in the list.
 * @res		: where to store the result.
 *
 * Walk the linked list of the supplied block allocation table and build up a
 * table for the list starting in @block.
 *
 * Returns TRUE on error.
 */
static gboolean
ole_make_bat (MSOleBAT const *metabat, size_t size_guess, guint32 block,
	      MSOleBAT *res)
{
	/* NOTE : Only use size as a suggestion, sometimes it is wrong */
	GArray *bat = g_array_sized_new (FALSE, FALSE,
		sizeof (guint32), size_guess);

	guint8 *used = (guint8*)g_alloca (1 + metabat->num_blocks / 8);
	memset (used, 0, 1 + metabat->num_blocks / 8);

	if (block < metabat->num_blocks)
		do {
			/* Catch cycles in the bat list */
			g_return_val_if_fail (0 == (used[block/8] & (1 << (block & 0x7))), TRUE);
			used[block/8] |= 1 << (block & 0x7);

			g_array_append_val (bat, block);
			block = metabat->block [block];
		} while (block < metabat->num_blocks);

	res->block = NULL;

	res->num_blocks = bat->len;
	res->block = (guint32 *) (gpointer) g_array_free (bat, FALSE);

	if (block != BAT_MAGIC_END_OF_CHAIN) {
#if 0
		g_warning ("This OLE2 file is invalid.\n"
			   "The Block Allocation  Table for one of the streams had %x instead of a terminator (%x).\n"
			   "We might still be able to extract some data, but you'll want to check the file.",
			   block, BAT_MAGIC_END_OF_CHAIN);
#endif
	}

	return FALSE;
}

static void
ols_bat_release (MSOleBAT *bat)
{
	if (bat->block != NULL) {
		g_free (bat->block);
		bat->block = NULL;
		bat->num_blocks = 0;
	}
}

/**
 * ole_info_read_metabat :
 * @ole  :
 * @bats :
 *
 * A small utility routine to read a set of references to bat blocks
 * either from the OLE header, or a meta-bat block.
 *
 * Returns a pointer to the element after the last position filled.
 **/
static guint32 *
ole_info_read_metabat (GsfInfileMSOle *ole, guint32 *bats, guint32 max,
		       guint32 const *metabat, guint32 const *metabat_end)
{
	guint8 const *bat, *end;

	for (; metabat < metabat_end; metabat++) {
		bat = ole_get_block (ole, *metabat, NULL);
		if (bat == NULL)
			return NULL;
		end = bat + ole->info->bb.size;
		for ( ; bat < end ; bat += BAT_INDEX_SIZE, bats++) {
			*bats = GSF_LE_GET_GUINT32 (bat);
			g_return_val_if_fail (*bats < max ||
					      *bats >= BAT_MAGIC_METABAT, NULL);
		}
	}
	return bats;
}

/**
 * gsf_ole_get_guint32s :
 * @dst :
 * @src :
 * @num_bytes :
 *
 * Copy some some raw data into an array of guint32.
 **/
static void
gsf_ole_get_guint32s (guint32 *dst, guint8 const *src, int num_bytes)
{
	for (; (num_bytes -= BAT_INDEX_SIZE) >= 0 ; src += BAT_INDEX_SIZE)
		*dst++ = GSF_LE_GET_GUINT32 (src);
}

static struct GsfInput *
ole_info_get_sb_file (GsfInfileMSOle *parent)
{
	MSOleBAT meta_sbat;

	if (parent->info->sb_file != NULL)
		return parent->info->sb_file;

	parent->info->sb_file = gsf_infile_msole_new_child (parent,
		parent->info->root_dir);

	if (NULL == parent->info->sb_file)
		return NULL;

	g_return_val_if_fail (parent->info->sb.bat.block == NULL, NULL);

	if (ole_make_bat (&parent->info->bb.bat,
			  parent->info->num_sbat,
                          parent->info->sbat_start,
                          &meta_sbat)) {
		return NULL;
	}

	parent->info->sb.bat.num_blocks = meta_sbat.num_blocks * (parent->info->bb.size / BAT_INDEX_SIZE);
	parent->info->sb.bat.block	= g_new0 (guint32, parent->info->sb.bat.num_blocks);
	ole_info_read_metabat (parent, parent->info->sb.bat.block,
		parent->info->sb.bat.num_blocks,
		meta_sbat.block, meta_sbat.block + meta_sbat.num_blocks);
	ols_bat_release (&meta_sbat);

	return parent->info->sb_file;
}

static gint
ole_dirent_cmp (const MSOleDirent *a, const MSOleDirent *b)
{
	g_return_val_if_fail (a, 0);
	g_return_val_if_fail (b, 0);

	g_return_val_if_fail (a->collation_name, 0);
	g_return_val_if_fail (b->collation_name, 0);

	return strcmp (b->collation_name, a->collation_name);
}

/**
 * ole_dirent_new :
 * @ole    :
 * @entry  :
 * @parent : optional
 *
 * Parse dirent number @entry and recursively handle its siblings and children.
 **/
static MSOleDirent *
ole_dirent_new (GsfInfileMSOle *ole, guint32 entry, MSOleDirent *parent)
{
	MSOleDirent *dirent;
	guint32 block, next, prev, child, size;
	guint8 const *data;
	guint8 type;
	guint16 name_len;

	if (entry >= DIRENT_MAGIC_END)
		return NULL;

	block = OLE_BIG_BLOCK (entry * DIRENT_SIZE, ole);

	g_return_val_if_fail (block < ole->bat.num_blocks, NULL);
	data = ole_get_block (ole, ole->bat.block [block], NULL);
	if (data == NULL)
		return NULL;
	data += (DIRENT_SIZE * entry) % ole->info->bb.size;

	type = GSF_LE_GET_GUINT8 (data + DIRENT_TYPE);
	if (type != DIRENT_TYPE_DIR &&
	    type != DIRENT_TYPE_FILE &&
	    type != DIRENT_TYPE_ROOTDIR) {
#if 0
		g_warning ("Unknown stream type 0x%x", type);
#endif
		return NULL;
	}

	/* It looks like directory (and root directory) sizes are sometimes bogus */
	size = GSF_LE_GET_GUINT32 (data + DIRENT_FILE_SIZE);
	g_return_val_if_fail (type == DIRENT_TYPE_DIR || type == DIRENT_TYPE_ROOTDIR ||
			      size <= (guint32)gsf_input_size(ole->input), NULL);

	dirent = g_new0 (MSOleDirent, 1);
	dirent->index	     = entry;
	dirent->size	     = size;
	/* Store the class id which is 16 byte identifier used by some apps */
	memcpy(dirent->clsid, data + DIRENT_CLSID, sizeof(dirent->clsid));

	/* root dir is always big block */
	dirent->use_sb	     = parent && (size < ole->info->threshold);
	dirent->first_block  = (GSF_LE_GET_GUINT32 (data + DIRENT_FIRSTBLOCK));
	dirent->is_directory = (type != DIRENT_TYPE_FILE);
	dirent->children     = NULL;
	prev  = GSF_LE_GET_GUINT32 (data + DIRENT_PREV);
	next  = GSF_LE_GET_GUINT32 (data + DIRENT_NEXT);
	child = GSF_LE_GET_GUINT32 (data + DIRENT_CHILD);
	name_len = GSF_LE_GET_GUINT16 (data + DIRENT_NAME_LEN);
	dirent->name = NULL;
	if (0 < name_len && name_len <= DIRENT_MAX_NAME_SIZE) {
		gunichar2 uni_name [DIRENT_MAX_NAME_SIZE+1];
		gchar const *end;
		int i;

		/* !#%!@$#^
		 * Sometimes, rarely, people store the stream name as ascii
		 * rather than utf16.  Do a validation first just in case.
		 */
		if (!g_utf8_validate ((const char*) data, -1, &end) ||
		    ((guint8 const *)end - data + 1) != name_len) {
			/* be wary about endianness */
			for (i = 0 ; i < name_len ; i += 2)
				uni_name [i/2] = GSF_LE_GET_GUINT16 (data + i);
			uni_name [i/2] = 0;

			dirent->name = g_utf16_to_utf8 (uni_name, -1, NULL, NULL, NULL);
		} else
			dirent->name = g_strndup ((gchar *)data, (gsize)((guint8 const *)end - data + 1));
	}
	/* be really anal in the face of screwups */
	if (dirent->name == NULL)
		dirent->name = g_strdup ("");
	dirent->collation_name = g_utf8_collate_key (dirent->name, -1);

	if (parent != NULL)
		parent->children = g_list_insert_sorted (parent->children,
			dirent, (GCompareFunc)ole_dirent_cmp);

	/* NOTE : These links are a tree, not a linked list */
	if (prev != entry)
		ole_dirent_new (ole, prev, parent);
	if (next != entry)
		ole_dirent_new (ole, next, parent);

	if (dirent->is_directory)
		ole_dirent_new (ole, child, dirent);
	return dirent;
}

static void
ole_dirent_free (MSOleDirent *dirent)
{
	GList *tmp;
	g_return_if_fail (dirent != NULL);

	g_free (dirent->name);
	g_free (dirent->collation_name);

	for (tmp = dirent->children; tmp; tmp = tmp->next)
		ole_dirent_free ((MSOleDirent *)tmp->data);
	g_list_free (dirent->children);
	g_free (dirent);
}

/*****************************************************************************/

static void
ole_info_unref (MSOleInfo *info)
{
	if (info->ref_count-- != 1)
		return;

	ols_bat_release (&info->bb.bat);
	ols_bat_release (&info->sb.bat);
	if (info->root_dir != NULL) {
		ole_dirent_free (info->root_dir);
		info->root_dir = NULL;
	}
	if (info->sb_file != NULL)  {
		gsf_input_finalize(info->sb_file);
		info->sb_file = NULL;
	}
	g_free (info);
}

static MSOleInfo *
ole_info_ref (MSOleInfo *info)
{
	info->ref_count++;
	return info;
}

static void
gsf_infile_msole_init (GsfInfileMSOle * ole)
{
	ole->cur_offset = 0;
	ole->size = 0;
	ole->input		= NULL;
	ole->info		= NULL;
	ole->bat.block		= NULL;
	ole->bat.num_blocks	= 0;
	ole->cur_block		= BAT_MAGIC_UNUSED;
	ole->stream.buf		= NULL;
	ole->stream.buf_size	= 0;
}

static void
gsf_infile_msole_finalize (GsfInfileMSOle * ole)
{
	if (ole->input != NULL) {
		gsf_input_finalize(ole->input);
		ole->input = NULL;
	}
	if (ole->info != NULL) {
		ole_info_unref (ole->info);
		ole->info = NULL;
	}
	ols_bat_release (&ole->bat);

	g_free (ole->stream.buf);
	free(ole);
}
	
/**
 * ole_dup :
 * @src :
 *
 * Utility routine to _partially_ replicate a file.  It does NOT copy the bat
 * blocks, or init the dirent.
 *
 * Return value: the partial duplicate.
 **/
static GsfInfileMSOle *
ole_dup (GsfInfileMSOle const * src)
{
	GsfInfileMSOle	*dst;
	struct GsfInput *input;

	g_return_val_if_fail (src != NULL, NULL);

	dst = malloc(sizeof(GsfInfileMSOle));
	if (dst == NULL)
		return NULL;
	gsf_infile_msole_init(dst);
	input = gsf_input_dup (src->input);
	if (input == NULL) {
		gsf_infile_msole_finalize(dst);
		return NULL;
	}
	dst->input = input;
	dst->info  = ole_info_ref (src->info);

	/* buf and buf_size are initialized to NULL */

	return dst;
}
	
/**
 * ole_init_info :
 * @ole :
 *
 * Read an OLE header and do some sanity checking
 * along the way.
 *
 * Return value: TRUE on error
 **/
static gboolean
ole_init_info (GsfInfileMSOle *ole)
{
	static guint8 const signature[] =
		{ 0xd0, 0xcf, 0x11, 0xe0, 0xa1, 0xb1, 0x1a, 0xe1 };
	guint8 const *header, *tmp;
	guint32 *metabat = NULL;
	MSOleInfo *info;
	guint32 bb_shift, sb_shift, num_bat, num_metabat, last, dirent_start;
	guint32 metabat_block, *ptr;

	/* check the header */
	if (gsf_input_seek (ole->input, (off_t) 0, SEEK_SET) ||
	    NULL == (header = gsf_input_read (ole->input, OLE_HEADER_SIZE, NULL)) ||
	    0 != memcmp (header, signature, sizeof (signature))) {
		return TRUE;
	}

	bb_shift      = GSF_LE_GET_GUINT16 (header + OLE_HEADER_BB_SHIFT);
	sb_shift      = GSF_LE_GET_GUINT16 (header + OLE_HEADER_SB_SHIFT);
	num_bat	      = GSF_LE_GET_GUINT32 (header + OLE_HEADER_NUM_BAT);
	dirent_start  = GSF_LE_GET_GUINT32 (header + OLE_HEADER_DIRENT_START);
        metabat_block = GSF_LE_GET_GUINT32 (header + OLE_HEADER_METABAT_BLOCK);
	num_metabat   = GSF_LE_GET_GUINT32 (header + OLE_HEADER_NUM_METABAT);

	/* Some sanity checks
	 * 1) There should always be at least 1 BAT block
	 * 2) It makes no sense to have a block larger than 2^31 for now.
	 *    Maybe relax this later, but not much.
	 */
	if (6 > bb_shift || bb_shift >= 31 || sb_shift > bb_shift) {
		return TRUE;
	}

	info = g_new0 (MSOleInfo, 1);
	ole->info = info;

	info->ref_count	     = 1;
	info->bb.shift	     = bb_shift;
	info->bb.size	     = 1 << info->bb.shift;
	info->bb.filter	     = info->bb.size - 1;
	info->sb.shift	     = sb_shift;
	info->sb.size	     = 1 << info->sb.shift;
	info->sb.filter	     = info->sb.size - 1;
	info->threshold	     = GSF_LE_GET_GUINT32 (header + OLE_HEADER_THRESHOLD);
        info->sbat_start     = GSF_LE_GET_GUINT32 (header + OLE_HEADER_SBAT_START);
        info->num_sbat       = GSF_LE_GET_GUINT32 (header + OLE_HEADER_NUM_SBAT);
	info->max_block	     = (gsf_input_size (ole->input) - OLE_HEADER_SIZE) / info->bb.size;
	info->sb_file	     = NULL;

	if (info->num_sbat == 0 && info->sbat_start != BAT_MAGIC_END_OF_CHAIN) {
#if 0
		g_warning ("There is are not supposed to be any blocks in the small block allocation table, yet there is a link to some.  Ignoring it.");
#endif
	}

	/* very rough heuristic, just in case */
	if (num_bat < info->max_block) {
		info->bb.bat.num_blocks = num_bat * (info->bb.size / BAT_INDEX_SIZE);
		info->bb.bat.block	= g_new0 (guint32, info->bb.bat.num_blocks);

		metabat = (guint32 *)g_alloca (MAX (info->bb.size, OLE_HEADER_SIZE));

		/* Reading the elements invalidates this memory, make copy */
		gsf_ole_get_guint32s (metabat, header + OLE_HEADER_START_BAT,
			OLE_HEADER_SIZE - OLE_HEADER_START_BAT);
		last = num_bat;
		if (last > OLE_HEADER_METABAT_SIZE)
			last = OLE_HEADER_METABAT_SIZE;

		ptr = ole_info_read_metabat (ole, info->bb.bat.block,
			info->bb.bat.num_blocks, metabat, metabat + last);
		num_bat -= last;
	} else
		ptr = NULL;

	last = (info->bb.size - BAT_INDEX_SIZE) / BAT_INDEX_SIZE;
	while (ptr != NULL && num_metabat-- > 0) {
		tmp = ole_get_block (ole, metabat_block, NULL);
		if (tmp == NULL) {
			ptr = NULL;
			break;
		}

		/* Reading the elements invalidates this memory, make copy */
		gsf_ole_get_guint32s (metabat, tmp, (int)info->bb.size);

		if (num_metabat == 0) {
			if (last < num_bat) {
				/* there should be less that a full metabat block
				 * remaining */
				ptr = NULL;
				break;
			}
			last = num_bat;
		} else if (num_metabat > 0) {
			metabat_block = metabat[last];
			num_bat -= last;
		}

		ptr = ole_info_read_metabat (ole, ptr,
			info->bb.bat.num_blocks, metabat, metabat + last);
	}

	if (ptr == NULL) {
		return TRUE;
	}

	/* Read the directory's bat, we do not know the size */
	if (ole_make_bat (&info->bb.bat, 0, dirent_start, &ole->bat)) {
		return TRUE;
	}

	/* Read the directory */
	ole->dirent = info->root_dir = ole_dirent_new (ole, 0, NULL);
	if (ole->dirent == NULL) {
		return TRUE;
	}

	return FALSE;
}

static guint8 const *
gsf_infile_msole_read (GsfInfileMSOle *ole, size_t num_bytes, guint8 *buffer)
{
	off_t first_block, last_block, raw_block, offset, i;
	guint8 const *data;
	guint8 *ptr;
	size_t count;

	/* small block files are preload */
	if (ole->dirent != NULL && ole->dirent->use_sb) {
		if (buffer != NULL) {
			memcpy (buffer, ole->stream.buf + ole->cur_offset, num_bytes);
			ole->cur_offset += num_bytes;
			return buffer;
		}
		data = ole->stream.buf + ole->cur_offset;
		ole->cur_offset += num_bytes;
		return data;
	}

	/* GsfInput guarantees that num_bytes > 0 */
	first_block = OLE_BIG_BLOCK (ole->cur_offset, ole);
	last_block = OLE_BIG_BLOCK (ole->cur_offset + num_bytes - 1, ole);
	offset = ole->cur_offset & ole->info->bb.filter;

	/* optimization : are all the raw blocks contiguous */
	i = first_block;
	raw_block = ole->bat.block [i];
	while (++i <= last_block && ++raw_block == ole->bat.block [i])
		;
	if (i > last_block) {
		/* optimization don't seek if we don't need to */
		if (ole->cur_block != first_block) {
			if (gsf_input_seek (ole->input,
				(off_t)(MAX (OLE_HEADER_SIZE, ole->info->bb.size) + (ole->bat.block [first_block] << ole->info->bb.shift) + offset),
				SEEK_SET) < 0)
				return NULL;
		}
		ole->cur_block = last_block;
		return gsf_input_read (ole->input, 
				       num_bytes,
				       (unsigned char*) buffer);
	}

	/* damn, we need to copy it block by block */
	if (buffer == NULL) {
		if (ole->stream.buf_size < num_bytes) {
			if (ole->stream.buf != NULL)
				g_free (ole->stream.buf);
			ole->stream.buf_size = num_bytes;
			ole->stream.buf = g_new (guint8, num_bytes);
		}
		buffer = ole->stream.buf;
	}

	ptr = buffer;
	for (i = first_block ; i <= last_block ; i++ , ptr += count, num_bytes -= count) {
		count = ole->info->bb.size - offset;
		if (count > num_bytes)
			count = num_bytes;
		data = ole_get_block (ole, ole->bat.block [i], NULL);
		if (data == NULL)
			return NULL;

		/* TODO : this could be optimized to avoid the copy */
		memcpy (ptr, data + offset, count);
		offset = 0;
	}
	ole->cur_block = BAT_MAGIC_UNUSED;
	ole->cur_offset += num_bytes;
	return buffer;
}
	
static struct GsfInput *
gsf_infile_msole_new_child (GsfInfileMSOle *parent,
			    MSOleDirent *dirent)
{
	GsfInfileMSOle * child;
	MSOleInfo *info;
	MSOleBAT const *metabat;
	struct GsfInput *sb_file = NULL;
	size_t size_guess;
	char * buf;
	

	if ( (dirent->index != 0) &&
	     (dirent->is_directory) ) {
		/* be wary.  It seems as if some implementations pretend that the
		 * directories contain data */
	  return gsf_input_new((const unsigned char*) "",
			       (off_t) 0,
			       0);
	}
	child = ole_dup (parent);
	if (child == NULL)
		return NULL;	
	child->dirent = dirent;
	child->size = (off_t) dirent->size;
		
	info = parent->info;

        if (dirent->use_sb) {	/* build the bat */
		metabat = &info->sb.bat;
		size_guess = dirent->size >> info->sb.shift;
		sb_file = ole_info_get_sb_file (parent);
	} else {
		metabat = &info->bb.bat;
		size_guess = dirent->size >> info->bb.shift;
	}
	if (ole_make_bat (metabat, size_guess + 1, dirent->first_block, &child->bat)) {
		gsf_infile_msole_finalize(child);
		return NULL;
	}

	if (dirent->use_sb) {
		unsigned i;
		guint8 const *data;
		
		if (sb_file == NULL) {
			gsf_infile_msole_finalize(child);
			return NULL;
		}

		child->stream.buf_size = info->threshold;
		child->stream.buf = g_new (guint8, info->threshold);

		for (i = 0 ; i < child->bat.num_blocks; i++)
			if (gsf_input_seek (sb_file,
					    (off_t)(child->bat.block [i] << info->sb.shift), SEEK_SET) < 0 ||
			    (data = gsf_input_read (sb_file,
						    info->sb.size,
				child->stream.buf + (i << info->sb.shift))) == NULL) {
				gsf_infile_msole_finalize(child);
				return NULL;
			}
	}
	buf = malloc(child->size);
	if (buf == NULL) {
		gsf_infile_msole_finalize(child);
		return NULL;
	}
	if (NULL == gsf_infile_msole_read(child,
					  child->size,
					  (guint8*) buf)) {
		gsf_infile_msole_finalize(child);	
		return NULL;
	}
	gsf_infile_msole_finalize(child);
	return gsf_input_new((const unsigned char*) buf,
			     (off_t) dirent->size,
			     1);
}
	

static struct GsfInput *
gsf_infile_msole_child_by_index (GsfInfileMSOle * ole, int target)
{
	GList *p;

	for (p = ole->dirent->children; p != NULL ; p = p->next)
		if (target-- <= 0)
			return gsf_infile_msole_new_child (ole,
				(MSOleDirent *)p->data);
	return NULL;
}

static char const *
gsf_infile_msole_name_by_index (GsfInfileMSOle * ole, int target)
{
	GList *p;

	for (p = ole->dirent->children; p != NULL ; p = p->next)
		if (target-- <= 0)
			return ((MSOleDirent *)p->data)->name;
	return NULL;
}

static int
gsf_infile_msole_num_children (GsfInfileMSOle * ole)
{
	g_return_val_if_fail (ole->dirent != NULL, -1);

	if (!ole->dirent->is_directory)
		return -1;
	return g_list_length (ole->dirent->children);
}


/**
 * gsf_infile_msole_new :
 * @source :
 *
 * Opens the root directory of an MS OLE file.
 * NOTE : adds a reference to @source
 *
 * Returns : the new ole file handler
 **/
static GsfInfileMSOle *
gsf_infile_msole_new (struct GsfInput *source)
{
	GsfInfileMSOle * ole;

	ole = malloc(sizeof(GsfInfileMSOle));
	if (ole == NULL)
		return NULL;
	gsf_infile_msole_init(ole);
	ole->input = source;
	ole->size = (off_t) 0;

	if (ole_init_info (ole)) {
		gsf_infile_msole_finalize(ole);
		return NULL;
	}

	return ole;
}






/* ******************************** main extraction code ************************ */

/* using libgobject, needs init! */
void __attribute__ ((constructor)) ole_gobject_init(void) {
 g_type_init();
}

static struct EXTRACTOR_Keywords *
addKeyword(EXTRACTOR_KeywordList *oldhead,
	   const char *phrase,
	   EXTRACTOR_KeywordType type) {
   EXTRACTOR_KeywordList * keyword;

   if (strlen(phrase) == 0)
     return oldhead;
   if (0 == strcmp(phrase, "\"\""))
     return oldhead;
   if (0 == strcmp(phrase, "\" \""))
     return oldhead;
   if (0 == strcmp(phrase, " "))
     return oldhead;
   keyword = (EXTRACTOR_KeywordList*) malloc(sizeof(EXTRACTOR_KeywordList));
   keyword->next = oldhead;
   keyword->keyword = strdup(phrase);
   keyword->keywordType = type;
   return keyword;
}


static guint8 const component_guid [] = {
	0xe0, 0x85, 0x9f, 0xf2, 0xf9, 0x4f, 0x68, 0x10,
	0xab, 0x91, 0x08, 0x00, 0x2b, 0x27, 0xb3, 0xd9
};

static guint8 const document_guid [] = {
	0x02, 0xd5, 0xcd, 0xd5, 0x9c, 0x2e, 0x1b, 0x10,
	0x93, 0x97, 0x08, 0x00, 0x2b, 0x2c, 0xf9, 0xae
};

static guint8 const user_guid [] = {
	0x05, 0xd5, 0xcd, 0xd5, 0x9c, 0x2e, 0x1b, 0x10,
	0x93, 0x97, 0x08, 0x00, 0x2b, 0x2c, 0xf9, 0xae
};

typedef enum {
	GSF_MSOLE_META_DATA_COMPONENT,
	GSF_MSOLE_META_DATA_DOCUMENT,
	GSF_MSOLE_META_DATA_USER
} GsfMSOleMetaDataType;

typedef enum {
	LE_VT_EMPTY               = 0,
	LE_VT_NULL                = 1,
	LE_VT_I2                  = 2,
	LE_VT_I4                  = 3,
	LE_VT_R4                  = 4,
	LE_VT_R8                  = 5,
	LE_VT_CY                  = 6,
	LE_VT_DATE                = 7,
	LE_VT_BSTR                = 8,
	LE_VT_DISPATCH            = 9,
	LE_VT_ERROR               = 10,
	LE_VT_BOOL                = 11,
	LE_VT_VARIANT             = 12,
	LE_VT_UNKNOWN             = 13,
	LE_VT_DECIMAL             = 14,
	LE_VT_I1                  = 16,
	LE_VT_UI1                 = 17,
	LE_VT_UI2                 = 18,
	LE_VT_UI4                 = 19,
	LE_VT_I8                  = 20,
	LE_VT_UI8                 = 21,
	LE_VT_INT                 = 22,
	LE_VT_UINT                = 23,
	LE_VT_VOID                = 24,
	LE_VT_HRESULT             = 25,
	LE_VT_PTR                 = 26,
	LE_VT_SAFEARRAY           = 27,
	LE_VT_CARRAY              = 28,
	LE_VT_USERDEFINED         = 29,
	LE_VT_LPSTR               = 30,
	LE_VT_LPWSTR              = 31,
	LE_VT_FILETIME            = 64,
	LE_VT_BLOB                = 65,
	LE_VT_STREAM              = 66,
	LE_VT_STORAGE             = 67,
	LE_VT_STREAMED_OBJECT     = 68,
	LE_VT_STORED_OBJECT       = 69,
	LE_VT_BLOB_OBJECT         = 70,
	LE_VT_CF                  = 71,
	LE_VT_CLSID               = 72,
	LE_VT_VECTOR              = 0x1000
} GsfMSOleVariantType;

typedef struct {
	char const *name;
	guint32	    id;
	GsfMSOleVariantType prefered_type;
} GsfMSOleMetaDataPropMap;

typedef struct {
	guint32		id;
	off_t	offset;
} GsfMSOleMetaDataProp;

typedef struct {
	GsfMSOleMetaDataType type;
	off_t   offset;
	guint32	    size, num_props;
	GIConv	    iconv_handle;
	unsigned    char_size;
	GHashTable *dict;
} GsfMSOleMetaDataSection;

static GsfMSOleMetaDataPropMap const document_props[] = {
	{ "Category",		2,	LE_VT_LPSTR },
	{ "PresentationFormat",	3,	LE_VT_LPSTR },
	{ "NumBytes",		4,	LE_VT_I4 },
	{ "NumLines",		5,	LE_VT_I4 },
	{ "NumParagraphs",	6,	LE_VT_I4 },
	{ "NumSlides",		7,	LE_VT_I4 },
	{ "NumNotes",		8,	LE_VT_I4 },
	{ "NumHiddenSlides",	9,	LE_VT_I4 },
	{ "NumMMClips",		10,	LE_VT_I4 },
	{ "Scale",		11,	LE_VT_BOOL },
	{ "HeadingPairs",	12,	LE_VT_VECTOR | LE_VT_VARIANT },
	{ "DocumentParts",	13,	LE_VT_VECTOR | LE_VT_LPSTR },
	{ "Manager",		14,	LE_VT_LPSTR },
	{ "Company",		15,	LE_VT_LPSTR },
	{ "LinksDirty",		16,	LE_VT_BOOL }
};

static GsfMSOleMetaDataPropMap const component_props[] = {
	{ "Title",		2,	LE_VT_LPSTR },
	{ "Subject",		3,	LE_VT_LPSTR },
	{ "Author",		4,	LE_VT_LPSTR },
	{ "Keywords",		5,	LE_VT_LPSTR },
	{ "Comments",		6,	LE_VT_LPSTR },
	{ "Template",		7,	LE_VT_LPSTR },
	{ "LastSavedBy",	8,	LE_VT_LPSTR },
	{ "RevisionNumber",	9,	LE_VT_LPSTR },
	{ "TotalEditingTime",	10,	LE_VT_FILETIME },
	{ "LastPrinted",	11,	LE_VT_FILETIME },
	{ "CreateTime",		12,	LE_VT_FILETIME },
	{ "LastSavedTime",	13,	LE_VT_FILETIME },
	{ "NumPages",		14,	LE_VT_I4 },
	{ "NumWords",		15,	LE_VT_I4 },
	{ "NumCharacters",	16,	LE_VT_I4 },
	{ "Thumbnail",		17,	LE_VT_CF },
	{ "AppName",		18,	LE_VT_LPSTR },
	{ "Security",		19,	LE_VT_I4 }
};

static GsfMSOleMetaDataPropMap const common_props[] = {
	{ "Dictionary",		0,	0, /* magic */},
	{ "CodePage",		1,	LE_VT_UI2 },
	{ "LOCALE_SYSTEM_DEFAULT",	0x80000000,	LE_VT_UI4},
	{ "CASE_SENSITIVE",		0x80000003,	LE_VT_UI4},
};

typedef struct {
  char * text;
  EXTRACTOR_KeywordType type;
} Matches;

static Matches tmap[] = {
  { "Title", EXTRACTOR_TITLE },
  { "PresentationFormat", EXTRACTOR_FORMAT },
  { "Category", EXTRACTOR_DESCRIPTION },
  { "Manager", EXTRACTOR_CREATED_FOR },
  { "Company", EXTRACTOR_ORGANIZATION },
  { "Subject", EXTRACTOR_SUBJECT },
  { "Author", EXTRACTOR_AUTHOR },
  { "Keywords", EXTRACTOR_KEYWORDS },
  { "Comments", EXTRACTOR_COMMENT },
  { "Template", EXTRACTOR_TEMPLATE },
  { "NumPages", EXTRACTOR_PAGE_COUNT },
  { "AppName", EXTRACTOR_SOFTWARE },
  { "RevisionNumber", EXTRACTOR_VERSIONNUMBER },
  { "Dictionary", EXTRACTOR_LANGUAGE },
  { "NumBytes", EXTRACTOR_SIZE },
  { "CreatedTime", EXTRACTOR_CREATION_DATE },
  { "LastSavedTime" , EXTRACTOR_MODIFICATION_DATE },
  { NULL, 0 },
};


static char const *
msole_prop_id_to_gsf (GsfMSOleMetaDataSection *section, guint32 id)
{
  char const *res = NULL;
  GsfMSOleMetaDataPropMap const *map = NULL;
  unsigned i = 0;

  if (section->dict != NULL) {
    if (id & 0x1000000) {
      id &= ~0x1000000;
      d (printf ("LINKED "););
    }

    res = g_hash_table_lookup (section->dict, GINT_TO_POINTER (id));

    if (res != NULL) {
      d (printf (res););
      return res;
    }
  }

  if (section->type == GSF_MSOLE_META_DATA_COMPONENT) {
    map = component_props;
    i = G_N_ELEMENTS (component_props);
  } else if (section->type == GSF_MSOLE_META_DATA_DOCUMENT) {
    map = document_props;
    i = G_N_ELEMENTS (document_props);
  }
  while (i-- > 0)
    if (map[i].id == id) {
      d (printf (map[i].name););
      return map[i].name;
    }

  map = common_props;
  i = G_N_ELEMENTS (common_props);
  while (i-- > 0)
    if (map[i].id == id) {
      d (printf (map[i].name););
      return map[i].name;
    }

  d (printf ("_UNKNOWN_(0x%x %d)", id, id););

  return NULL;
}

static GValue *
msole_prop_parse(GsfMSOleMetaDataSection *section,
		 guint32 type,
		 guint8 const **data,
		 guint8 const *data_end)
{
  GValue *res;
  char *str;
  guint32 len;
  gboolean const is_vector = type & LE_VT_VECTOR;
  GError * error;

  g_return_val_if_fail (!(type & (unsigned)(~0x1fff)), NULL); /* not valid in a prop set */

  type &= 0xfff;

  if (is_vector) {
    unsigned i, n;

    g_return_val_if_fail (*data + 4 <= data_end, NULL);

    n = GSF_LE_GET_GUINT32 (*data);
    *data += 4;

    d (printf (" array with %d elem\n", n););
    for (i = 0 ; i < n ; i++) {
      GValue *v;
      d (printf ("\t[%d] ", i););
      v = msole_prop_parse (section, type, data, data_end);
      if (v) {
	/* FIXME: do something with it.  */
	if (G_IS_VALUE (v))
	  g_value_unset (v);
	g_free (v);
      }
    }
    return NULL;
  }

  res = g_new0 (GValue, 1);
  switch (type) {
  case LE_VT_EMPTY :		 d (puts ("VT_EMPTY"););
    /* value::unset == empty */
    break;

  case LE_VT_NULL :		 d (puts ("VT_NULL"););
    /* value::unset == null too :-) do we need to distinguish ? */
    break;

  case LE_VT_I2 :		 d (puts ("VT_I2"););
    g_return_val_if_fail (*data + 2 <= data_end, NULL);
    g_value_init (res, G_TYPE_INT);
    g_value_set_int	(res, GSF_LE_GET_GINT16 (*data));
    *data += 2;
    break;

  case LE_VT_I4 :		 d (puts ("VT_I4"););
    g_return_val_if_fail (*data + 4 <= data_end, NULL);
    g_value_init (res, G_TYPE_INT);
    g_value_set_int	(res, GSF_LE_GET_GINT32 (*data));
    *data += 4;
    break;

  case LE_VT_R4 :		 d (puts ("VT_R4"););
    g_return_val_if_fail (*data + 4 <= data_end, NULL);
    g_value_init (res, G_TYPE_FLOAT);
    g_value_set_float (res, GSF_LE_GET_FLOAT (*data));
    *data += 4;
    break;

  case LE_VT_R8 :		 d (puts ("VT_R8"););
    g_return_val_if_fail (*data + 8 <= data_end, NULL);
    g_value_init (res, G_TYPE_DOUBLE);
    g_value_set_double (res, GSF_LE_GET_DOUBLE (*data));
    *data += 8;
    break;

  case LE_VT_CY :		 d (puts ("VT_CY"););
    /* 8-byte two's complement integer (scaled by 10,000) */
    /* CHEAT : just store as an int64 for now */
    g_return_val_if_fail (*data + 8 <= data_end, NULL);
    g_value_init (res, G_TYPE_INT64);
    g_value_set_int64 (res, GSF_LE_GET_GINT64 (*data));
    break;

  case LE_VT_DATE :		 d (puts ("VT_DATE"););
    break;

  case LE_VT_BSTR :		 d (puts ("VT_BSTR"););
    break;

  case LE_VT_DISPATCH :	 d (puts ("VT_DISPATCH"););
    break;

  case LE_VT_BOOL :		 d (puts ("VT_BOOL"););
    g_return_val_if_fail (*data + 1 <= data_end, NULL);
    g_value_init (res, G_TYPE_BOOLEAN);
    g_value_set_boolean (res, **data ? TRUE : FALSE);
    *data += 1;
    break;

  case LE_VT_VARIANT :	 d (printf ("VT_VARIANT containing a "););
    g_free (res);
    type = GSF_LE_GET_GUINT32 (*data);
    *data += 4;
    return msole_prop_parse (section, type, data, data_end);

  case LE_VT_UI1 :		 d (puts ("VT_UI1"););
    g_return_val_if_fail (*data + 1 <= data_end, NULL);
    g_value_init (res, G_TYPE_UCHAR);
    g_value_set_uchar (res, (guchar)(**data));
    *data += 1;
    break;

  case LE_VT_UI2 :		 d (puts ("VT_UI2"););
    g_return_val_if_fail (*data + 2 <= data_end, NULL);
    g_value_init (res, G_TYPE_UINT);
    g_value_set_uint (res, GSF_LE_GET_GUINT16 (*data));
    *data += 2;
    break;

  case LE_VT_UI4 :		 d (puts ("VT_UI4"););
    g_return_val_if_fail (*data + 4 <= data_end, NULL);
    g_value_init (res, G_TYPE_UINT);
    *data += 4;
    d (printf ("%u\n", GSF_LE_GET_GUINT32 (*data)););
    break;

  case LE_VT_I8 :		 d (puts ("VT_I8"););
    g_return_val_if_fail (*data + 8 <= data_end, NULL);
    g_value_init (res, G_TYPE_INT64);
    g_value_set_int64 (res, GSF_LE_GET_GINT64 (*data));
     *data += 8;
    break;

  case LE_VT_UI8 :		 d (puts ("VT_UI8"););
    g_return_val_if_fail (*data + 8 <= data_end, NULL);
    g_value_init (res, G_TYPE_UINT64);
    g_value_set_uint64 (res, GSF_LE_GET_GUINT64 (*data));
    *data += 8;
    break;

  case LE_VT_LPSTR :		 d (puts ("VT_LPSTR"););
    /*
     * This is the representation of many strings.  It is stored in
     * the same representation as VT_BSTR.  Note that the serialized
     * representation of VP_LPSTR has a preceding byte count, whereas
     * the in-memory representation does not.
     */
    /* be anal and safe */
    g_return_val_if_fail (*data + 4 <= data_end, NULL);
    
    len = GSF_LE_GET_GUINT32 (*data);
    
    g_return_val_if_fail (len < 0x10000, NULL);
    g_return_val_if_fail (*data + 4 + len*section->char_size <= data_end, NULL);
    
    error = NULL;
    d (gsf_mem_dump (*data + 4, len * section->char_size););
    str = g_convert_with_iconv ((char*) *data + 4,
				len * section->char_size,
				section->iconv_handle, NULL, NULL, &error);
    
    g_value_init (res, G_TYPE_STRING);
    if (NULL != str) {
      g_value_set_string (res, str);
      g_free (str);
    } else if (NULL != error) {
      g_warning ("error: %s", error->message);
      g_error_free (error);
    } else {
      g_warning ("unknown error converting string property, using blank");
    }
    *data += 4 + len * section->char_size;
    break;

  case LE_VT_LPWSTR : d (puts ("VT_LPWSTR"););
    /*
     * A counted and null-terminated Unicode string; a DWORD character
     * count (where the count includes the terminating null) followed
     * by that many Unicode (16-bit) characters.  Note that the count
     * is character count, not byte count.
     */
    /* be anal and safe */
    g_return_val_if_fail (*data + 4 <= data_end, NULL);
    
    len = GSF_LE_GET_GUINT32 (*data);
    
    g_return_val_if_fail (len < 0x10000, NULL);
    g_return_val_if_fail (*data + 4 + len <= data_end, NULL);
    
    error = NULL;
    d (gsf_mem_dump (*data + 4, len*2););
    str = g_convert ((char*) *data + 4, 
		     len*2,
		     "UTF-8", 
		     "UTF-16LE",
		     NULL, 
		     NULL, 
		     &error);
    
    g_value_init (res, G_TYPE_STRING);
    if (NULL != str) {
      g_value_set_string (res, str);
      g_free (str);
    } else if (NULL != error) {
      g_warning ("error: %s", error->message);
      g_error_free (error);
    } else {
      g_warning ("unknown error converting string property, using blank");
    }
    *data += 4 + len*2;
    break;

  case LE_VT_FILETIME :	 d (puts ("VT_FILETIME"););

    g_return_val_if_fail (*data + 8 <= data_end, NULL);

    g_value_init (res, G_TYPE_STRING);
    {
      /* ft * 100ns since Jan 1 1601 */
      guint64 ft = GSF_LE_GET_GUINT64 (*data);

      ft /= 10000000; /* convert to seconds */
#ifdef _MSC_VER
      ft -= 11644473600i64; /* move to Jan 1 1970 */
#else
      ft -= 11644473600ULL; /* move to Jan 1 1970 */
#endif

      str = g_strdup(ctime((time_t*)&ft));

      g_value_set_string (res, str);

      *data += 8;
      break;
    }
  case LE_VT_BLOB :		 d (puts ("VT_BLOB"););
    g_free (res);
    res = NULL;
    break;
  case LE_VT_STREAM :	 d (puts ("VT_STREAM"););
    g_free (res);
    res = NULL;
     break;
  case LE_VT_STORAGE :	 d (puts ("VT_STORAGE"););
    g_free (res);
    res = NULL;
    break;
  case LE_VT_STREAMED_OBJECT: d (puts ("VT_STREAMED_OBJECT"););
    g_free (res);
    res = NULL;
    break;
  case LE_VT_STORED_OBJECT :	 d (puts ("VT_STORED_OBJECT"););
    g_free (res);
    res = NULL;
    break;
  case LE_VT_BLOB_OBJECT :	 d (puts ("VT_BLOB_OBJECT"););
    g_free (res);
    res = NULL;
    break;
  case LE_VT_CF :		 d (puts ("VT_CF"););
    break;
  case LE_VT_CLSID :		 d (puts ("VT_CLSID"););
    *data += 16;
    g_free (res);
    res = NULL;
    break;

  case LE_VT_ERROR :
  case LE_VT_UNKNOWN :
  case LE_VT_DECIMAL :
  case LE_VT_I1 :
  case LE_VT_INT :
  case LE_VT_UINT :
  case LE_VT_VOID :
  case LE_VT_HRESULT :
  case LE_VT_PTR :
  case LE_VT_SAFEARRAY :
  case LE_VT_CARRAY :
  case LE_VT_USERDEFINED :
    warning ("type %d (0x%x) is not permitted in property sets",
	       type, type);
    g_free (res);
    res = NULL;
    break;

  default :
    warning ("Unknown property type %d (0x%x)", type, type);
    g_free (res);
    res = NULL;
  };

  d ( if (res != NULL && G_IS_VALUE (res)) {
    char *val = g_strdup_value_contents (res);
    d(printf ("%s\n", val););
    g_free (val);
  } else
      puts ("<unparsed>\n");
      );
  return res;
}

static GValue *
msole_prop_read (struct GsfInput *in,
		 GsfMSOleMetaDataSection *section,
		 GsfMSOleMetaDataProp    *props,
		 unsigned i)
{
  guint32 type;
  guint8 const *data;
  /* TODO : why size-4 ? I must be missing something */
  off_t size = ((i+1) >= section->num_props)
    ? section->size-4 : props[i+1].offset;
  char const *prop_name;

  g_return_val_if_fail (i < section->num_props, NULL);
  g_return_val_if_fail (size >= props[i].offset + 4, NULL);

  size -= props[i].offset; /* includes the type id */
  if (gsf_input_seek (in, section->offset+props[i].offset, SEEK_SET) ||
      NULL == (data = gsf_input_read (in, size, NULL))) {
    warning ("failed to read prop #%d", i);
    return NULL;
  }

  type = GSF_LE_GET_GUINT32 (data);
  data += 4;

  /* dictionary is magic */
  if (props[i].id == 0) {
    guint32 len, id, i, n;
    gsize gslen;
    char *name;
    guint8 const *start = data;

    g_return_val_if_fail (section->dict == NULL, NULL);

    section->dict = g_hash_table_new_full (
					   g_direct_hash, g_direct_equal,
					   NULL, g_free);

    n = type;
    for (i = 0 ; i < n ; i++) {
      id = GSF_LE_GET_GUINT32 (data);
      len = GSF_LE_GET_GUINT32 (data + 4);

      g_return_val_if_fail (len < 0x10000, NULL);

      gslen = 0;
      name = g_convert_with_iconv ((char*) data + 8,
				   len * section->char_size,
				   section->iconv_handle, &gslen, NULL, NULL);

      len = (guint32)gslen;
      data += 8 + len;

      d (printf ("\t%u == %s\n", id, name););
      g_hash_table_replace (section->dict,
			    GINT_TO_POINTER (id), name);

      /* MS documentation blows goats !
       * The docs claim there are padding bytes in the dictionary.
       * Their examples show padding bytes.
       * In reality non-unicode strings do not see to have padding.
       */
      if (section->char_size != 1 && (data - start) % 4)
	data += 4 - ((data - start) % 4);
    }

    return NULL;
  }

  d (printf ("%u) ", i););
  prop_name = msole_prop_id_to_gsf (section, props[i].id);

  d (printf (" @ %x %x = ", (unsigned)props[i].offset, (unsigned)size););
  return msole_prop_parse (section, type, &data, data + size);
}

static int
msole_prop_cmp (gconstpointer a, gconstpointer b)
{
  GsfMSOleMetaDataProp const *prop_a = a ;
  GsfMSOleMetaDataProp const *prop_b = b ;
  return prop_a->offset - prop_b->offset;
}

/**
 * gsf_msole_iconv_open_codepage_for_import :
 * @to:
 * @codepage :
 *
 * Returns an iconv converter for @codepage -> utf8.
 **/
static GIConv
gsf_msole_iconv_open_codepage_for_import(char const *to,
					 int codepage)
{
  GIConv iconv_handle;

  g_return_val_if_fail (to != NULL, (GIConv)(-1));
  /* sometimes it is stored as signed short */
  if (codepage == 65001 || codepage == -535) {
    iconv_handle = g_iconv_open (to, "UTF-8");
    if (iconv_handle != (GIConv)(-1))
      return iconv_handle;
  } else if (codepage != 1200 && codepage != 1201) {
    char* src_charset = g_strdup_printf ("CP%d", codepage);
    iconv_handle = g_iconv_open (to, src_charset);
    g_free (src_charset);
    if (iconv_handle != (GIConv)(-1))
      return iconv_handle;
  } else {
    char const *from = (codepage == 1200) ? "UTF-16LE" : "UTF-16BE";
    iconv_handle = g_iconv_open (to, from);
    if (iconv_handle != (GIConv)(-1))
      return iconv_handle;
  }

  /* Try aliases.  */
  if (codepage == 10000) {
    /* gnu iconv.  */
    iconv_handle = g_iconv_open (to, "MACROMAN");
    if (iconv_handle != (GIConv)(-1))
      return iconv_handle;

    /* glibc.  */
    iconv_handle = g_iconv_open (to, "MACINTOSH");
    if (iconv_handle != (GIConv)(-1))
      return iconv_handle;
  }

  warning ("Unable to open an iconv handle from codepage %d -> %s",
	     codepage, to);
  return (GIConv)(-1);
}

/**
 * gsf_msole_iconv_open_for_import :
 * @codepage :
 *
 * Returns an iconv converter for single byte encodings @codepage -> utf8.
 * 	Attempt to handle the semantics of a specification for multibyte encodings
 * 	since this is only supposed to be used for single bytes.
 **/
static GIConv
gsf_msole_iconv_open_for_import (int codepage)
{
  return gsf_msole_iconv_open_codepage_for_import ("UTF-8", codepage);
}





static struct EXTRACTOR_Keywords * process(struct GsfInput * in,
					   struct EXTRACTOR_Keywords * prev) {
  guint8 const *data = gsf_input_read (in, 28, NULL);
  guint16 version;
  guint32 os, num_sections;
  unsigned i, j;
  GsfMSOleMetaDataSection *sections;
  GsfMSOleMetaDataProp *props;

  if (NULL == data)
    return prev;

  /* NOTE : high word is the os, low word is the os version
   * 0 = win16
   * 1 = mac
   * 2 = win32
   */
  os = GSF_LE_GET_GUINT16 (data + 6);

  version = GSF_LE_GET_GUINT16 (data + 2);

  num_sections = GSF_LE_GET_GUINT32 (data + 24);
  if (GSF_LE_GET_GUINT16 (data + 0) != 0xfffe
      || (version != 0 && version != 1)
      || os > 2
      || num_sections > 100) { /* arbitrary sanity check */
    return prev;
  }

  /* extract the section info */
  sections = (GsfMSOleMetaDataSection *)g_alloca (sizeof (GsfMSOleMetaDataSection)* num_sections);
  for (i = 0 ; i < num_sections ; i++) {
    data = gsf_input_read (in, 20, NULL);
    if (NULL == data) {
      return prev;
    }
    if (!memcmp (data, component_guid, sizeof (component_guid)))
      sections [i].type = GSF_MSOLE_META_DATA_COMPONENT;
    else if (!memcmp (data, document_guid, sizeof (document_guid)))
      sections [i].type = GSF_MSOLE_META_DATA_DOCUMENT;
    else if (!memcmp (data, user_guid, sizeof (user_guid)))
      sections [i].type = GSF_MSOLE_META_DATA_USER;
    else {
      sections [i].type = GSF_MSOLE_META_DATA_USER;
      warning ("Unknown property section type, treating it as USER");
    }

    sections [i].offset = GSF_LE_GET_GUINT32 (data + 16);
#ifndef NO_DEBUG_OLE_PROPS
    d(printf ("0x%x\n", (guint32)sections [i].offset););
#endif
  }
  for (i = 0 ; i < num_sections ; i++) {
    if (gsf_input_seek (in, sections[i].offset, SEEK_SET) ||
	NULL == (data = gsf_input_read (in, 8, NULL))) {
      return prev;
    }

    sections[i].iconv_handle = (GIConv)-1;
    sections[i].char_size    = 1;
    sections[i].dict      = NULL;
    sections[i].size      = GSF_LE_GET_GUINT32 (data); /* includes header */
    sections[i].num_props = GSF_LE_GET_GUINT32 (data + 4);
    if (sections[i].num_props <= 0)
      continue;
    props = g_new (GsfMSOleMetaDataProp, sections[i].num_props);
    for (j = 0; j < sections[i].num_props; j++) {
      if (NULL == (data = gsf_input_read (in, 8, NULL))) {
	g_free (props);
	return prev;
      }

      props [j].id = GSF_LE_GET_GUINT32 (data);
      props [j].offset  = GSF_LE_GET_GUINT32 (data + 4);
    }

    /* order prop info by offset to facilitate bounds checking */
    qsort (props, sections[i].num_props,
	   sizeof (GsfMSOleMetaDataProp),
	   msole_prop_cmp);

    sections[i].iconv_handle = (GIConv)-1;
    sections[i].char_size = 1;
    for (j = 0; j < sections[i].num_props; j++) /* first codepage */
      if (props[j].id == 1) {
	GValue *v = msole_prop_read (in, sections+i, props, j);
	if (v != NULL) {
	  if (G_IS_VALUE (v)) {
	    if (G_VALUE_HOLDS_INT (v)) {
	      int codepage = g_value_get_int (v);
	      sections[i].iconv_handle = gsf_msole_iconv_open_for_import (codepage);
	      if (codepage == 1200 || codepage == 1201)
		sections[i].char_size = 2;
	    }
	    g_value_unset (v);
	  }
	  g_free (v) ;
	}
      }
    if (sections[i].iconv_handle == (GIConv)-1)
      sections[i].iconv_handle = gsf_msole_iconv_open_for_import (1252);

    for (j = 0; j < sections[i].num_props; j++) /* then dictionary */
      if (props[j].id == 0) {
	GValue *v = msole_prop_read (in, sections+i, props, j);
	if (v) {
	  if (G_VALUE_TYPE(v) == G_TYPE_STRING) {
	    gchar * contents = g_strdup_value_contents(v);
	    free(contents);
	  } else {	
	
	    /* FIXME: do something with non-strings...  */
	  }
	  if (G_IS_VALUE (v))
	    g_value_unset (v);
	  g_free (v);
	}
      }
    for (j = 0; j < sections[i].num_props; j++) /* the rest */
      if (props[j].id > 1) {	
	GValue *v = msole_prop_read (in, sections+i, props, j);
	if (v && G_IS_VALUE(v)) {
	  gchar * contents = NULL;
	  int pc;
	  int ipc;
	
	  if (G_VALUE_TYPE(v) == G_TYPE_STRING) {
	    contents = strdup(g_value_get_string(v));
	  } else {
	    /* convert other formats? */
	    contents = g_strdup_value_contents(v);
	  }	
	  pc = 0;
	  if (contents != NULL) {
	    for (ipc=strlen(contents)-1;ipc>=0;ipc--)
	      if ( (isprint(contents[ipc])) &&
		   (! isspace(contents[ipc])) )
		pc++;
	    if ( (strlen(contents) > 0) &&
		 (contents[strlen(contents)-1] == '\n') )
		 contents[strlen(contents)-1] = '\0';
	  }
	  if (pc > 0) {
	    int pos = 0;
	    const char * prop
	      = msole_prop_id_to_gsf(sections+i, props[j].id);
	    if (prop != NULL) {
	      while (tmap[pos].text != NULL) {
		if (0 == strcmp(tmap[pos].text,
				prop))
		  break;
		pos++;
	      }
	      if (tmap[pos].text != NULL)
		prev = addKeyword(prev,
				  contents,
				  tmap[pos].type);
	    }
	  }
	  if (contents != NULL)
	    free(contents);	
	}
	if (v) {
	  if (G_IS_VALUE (v))
	    g_value_unset (v);
	  g_free (v);
	}
      }

    gsf_iconv_close (sections[i].iconv_handle);
    g_free (props);
    if (sections[i].dict != NULL)
      g_hash_table_destroy (sections[i].dict);
  }
  switch (os) {
  case 0:
    prev = addKeyword(prev,
		      "Win16",
		      EXTRACTOR_OS);
    break;
  case 1:
    prev = addKeyword(prev,
		      "MacOS",
		      EXTRACTOR_OS);
    break;
  case 2:
    prev = addKeyword(prev,
		      "Win32",
		      EXTRACTOR_OS);
    break;
  }
  return prev;
}

static struct EXTRACTOR_Keywords * processSO(struct GsfInput * src,
					     struct EXTRACTOR_Keywords * prev) {
  off_t size;
  char * buf;

  size = gsf_input_size(src);
  if (size < 0x374) /* == 0x375?? */
    return prev;
  buf = malloc(size);
  gsf_input_read(src, size, (unsigned char*) buf);
  if ( (buf[0] != 0x0F) ||
       (buf[1] != 0x0) ||
       (0 != strncmp(&buf[2],
		     "SfxDocumentInfo",
		     strlen("SfxDocumentInfo"))) ||
       (buf[0x11] != 0x0B) ||
       (buf[0x13] != 0x00) || /* pw protected! */
       (buf[0x12] != 0x00) ) {
    free(buf);
    return prev;
  }
  buf[0xd3] = '\0';
  if (buf[0x94] + buf[0x93] > 0)
    prev = addKeyword(prev,
		      &buf[0x95],
		      EXTRACTOR_TITLE);
  buf[0x114] = '\0';
  if (buf[0xd5] + buf[0xd4] > 0)
    prev = addKeyword(prev,
		      &buf[0xd6],
		      EXTRACTOR_SUBJECT);
  buf[0x215] = '\0';
  if (buf[0x115] + buf[0x116] > 0)
    prev = addKeyword(prev,
		      &buf[0x117],
		      EXTRACTOR_COMMENT);
  buf[0x296] = '\0';
  if (buf[0x216] + buf[0x217] > 0)
    prev = addKeyword(prev,
		      &buf[0x218],
		      EXTRACTOR_KEYWORDS);
  /* fixme: do timestamps,
     mime-type, user-defined info's */

  free(buf);
  return prev;
}

struct EXTRACTOR_Keywords *
libextractor_ole2_extract(const char * filename,
			  const char * date,
			  size_t size,
			  struct EXTRACTOR_Keywords * prev) {
  struct GsfInput   *input;
  struct GsfInfileMSOle * infile;
  struct GsfInput * src;
  const char * name;
  const char * software = 0;
  int i;

  input = gsf_input_new((const unsigned char*) date,
			(off_t) size,
			0);
  if (input == NULL)
    return prev;

  infile = gsf_infile_msole_new(input);
  if (infile == NULL)
    return prev;

  for (i=0;i<gsf_infile_msole_num_children(infile);i++) {
    name = gsf_infile_msole_name_by_index (infile, i);
    src = NULL;
    if (name == NULL)
      continue;
    if ( (0 == strcmp(name, "\005SummaryInformation"))
	 || (0 == strcmp(name, "\005DocumentSummaryInformation")) ) {
      src = gsf_infile_msole_child_by_index (infile, i);
      if (src != NULL)
	prev = process(src,
		       prev);
    }
    if (0 == strcmp(name, "SfxDocumentInfo")) {
      src = gsf_infile_msole_child_by_index (infile, i);
      if (src != NULL)
	prev = processSO(src,
			 prev);
    }
    if (src != NULL)
      gsf_input_finalize(src);
  }
  gsf_infile_msole_finalize(infile);

  /*
   * Hack to return an appropriate mimetype
   */
  software = EXTRACTOR_extractLast(EXTRACTOR_SOFTWARE, prev);
  if(NULL != software) {
    const char * mimetype = "application/vnd.ms-files";
 
    if((0 == strncmp(software, "Microsoft Word", 14)) ||
       (0 == strncmp(software, "Microsoft Office Word", 21)))
      mimetype = "application/msword";
    else if((0 == strncmp(software, "Microsoft Excel", 15)) ||
            (0 == strncmp(software, "Microsoft Office Excel", 22)))
      mimetype = "application/vnd.ms-excel";
    else if((0 == strncmp(software, "Microsoft PowerPoint", 20)) ||
            (0 == strncmp(software, "Microsoft Office PowerPoint", 27)))
      mimetype = "application/vnd.ms-powerpoint";
    else if(0 == strncmp(software, "Microsoft Office", 16))
      mimetype = "application/vnd.ms-office";
  
    prev = addKeyword(prev, mimetype, EXTRACTOR_MIMETYPE);
  }

  return prev;
}

/* end of ole2extractor.c */
