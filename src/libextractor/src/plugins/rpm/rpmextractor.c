/*
     This file is part of libextractor.
     (C) 2002, 2003 Vidyut Samanta and Christian Grothoff

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

/* someone was using this non-portable function.  Mac OS X
 * doesn't have it so rpmextractor wouldn't load.  I rewrite
 * the function so that it will now work.  below is the
 * original FIXME notice.  -- Filip Pizlo 2003 */
/* FIXME: replace use of stpcpy to increase portability */
static char *my_stpcpy(char *dest, const char *src) {
  strcpy(dest,src);
  return dest+strlen(src);
}

/* **************** buffer-based IO ************** */

typedef struct {
  char * data;
  size_t pos;
  size_t len;
} fdStruct;

typedef fdStruct * FD_t;

static int timedRead(FD_t f,
		     void * dst,
		     size_t n) {
  size_t min;

  if (f->len - f->pos >= n)
    min = n;
  else
    min = f->len - f->pos;
  memcpy(dst,
	 &f->data[f->pos],
	 min);
  f->pos += min;
  return min;
}

/* *************** RPM types ************************ */

typedef int int_32;
typedef unsigned int uint_32;


/**
 * Header private tags.
 * @note General use tags should start at 1000 (RPM's tag space starts there).
 */
#define	HEADER_IMAGE		61
#define	HEADER_SIGNATURES	62
#define	HEADER_IMMUTABLE	63
#define	HEADER_REGIONS		64
#define HEADER_I18NTABLE	100
#define	HEADER_SIGBASE		256
#define	HEADER_TAGBASE		1000

/**
 * Tags identify data in package headers.
 * @note tags should not have value 0!
 */
typedef enum rpmTag_e {

    RPMTAG_HEADERIMAGE		= HEADER_IMAGE,		/*!< Current image. */
    RPMTAG_HEADERSIGNATURES	= HEADER_SIGNATURES,	/*!< Signatures. */
    RPMTAG_HEADERIMMUTABLE	= HEADER_IMMUTABLE,	/*!< Original image. */
/*@-enummemuse@*/
    RPMTAG_HEADERREGIONS	= HEADER_REGIONS,	/*!< Regions. */

    RPMTAG_HEADERI18NTABLE	= HEADER_I18NTABLE, /*!< I18N string locales. */
/*@=enummemuse@*/

/* Retrofit (and uniqify) signature tags for use by tagName() and rpmQuery. */
/* the md5 sum was broken *twice* on big endian machines */
/* XXX 2nd underscore prevents tagTable generation */
    RPMTAG_SIG_BASE		= HEADER_SIGBASE,
    RPMTAG_SIGSIZE		= RPMTAG_SIG_BASE+1,
    RPMTAG_SIGLEMD5_1		= RPMTAG_SIG_BASE+2,	/*!< internal - obsolate */
    RPMTAG_SIGPGP		= RPMTAG_SIG_BASE+3,
    RPMTAG_SIGLEMD5_2		= RPMTAG_SIG_BASE+4,	/*!< internal - obsolate */
    RPMTAG_SIGMD5	        = RPMTAG_SIG_BASE+5,
    RPMTAG_SIGGPG	        = RPMTAG_SIG_BASE+6,
    RPMTAG_SIGPGP5	        = RPMTAG_SIG_BASE+7,	/*!< internal - obsolate */

    RPMTAG_BADSHA1_1		= RPMTAG_SIG_BASE+8,	/*!< internal - obsolate */
    RPMTAG_BADSHA1_2		= RPMTAG_SIG_BASE+9, 	/*!< internal - obsolate */

    RPMTAG_PUBKEYS		= RPMTAG_SIG_BASE+10,
    RPMTAG_DSAHEADER		= RPMTAG_SIG_BASE+11,
    RPMTAG_RSAHEADER		= RPMTAG_SIG_BASE+12,
    RPMTAG_SHA1HEADER		= RPMTAG_SIG_BASE+13,

    RPMTAG_NAME  		= 1000,
    RPMTAG_VERSION		= 1001,
    RPMTAG_RELEASE		= 1002,
    RPMTAG_EPOCH   		= 1003,
#define	RPMTAG_SERIAL	RPMTAG_EPOCH	/* backward comaptibility */
    RPMTAG_SUMMARY		= 1004,
    RPMTAG_DESCRIPTION		= 1005,
    RPMTAG_BUILDTIME		= 1006,
    RPMTAG_BUILDHOST		= 1007,
    RPMTAG_INSTALLTIME		= 1008,
    RPMTAG_SIZE			= 1009,
    RPMTAG_DISTRIBUTION		= 1010,
    RPMTAG_VENDOR		= 1011,
    RPMTAG_GIF			= 1012,
    RPMTAG_XPM			= 1013,
    RPMTAG_LICENSE		= 1014,
#define	RPMTAG_COPYRIGHT RPMTAG_LICENSE	/* backward comaptibility */
    RPMTAG_PACKAGER		= 1015,
    RPMTAG_GROUP		= 1016,
/*@-enummemuse@*/
    RPMTAG_CHANGELOG		= 1017, /*!< internal */
/*@=enummemuse@*/
    RPMTAG_SOURCE		= 1018,
    RPMTAG_PATCH		= 1019,
    RPMTAG_URL			= 1020,
    RPMTAG_OS			= 1021,
    RPMTAG_ARCH			= 1022,
    RPMTAG_PREIN		= 1023,
    RPMTAG_POSTIN		= 1024,
    RPMTAG_PREUN		= 1025,
    RPMTAG_POSTUN		= 1026,
    RPMTAG_OLDFILENAMES		= 1027, /* obsolete */
    RPMTAG_FILESIZES		= 1028,
    RPMTAG_FILESTATES		= 1029,
    RPMTAG_FILEMODES		= 1030,
    RPMTAG_FILEUIDS		= 1031, /*!< internal */
    RPMTAG_FILEGIDS		= 1032, /*!< internal */
    RPMTAG_FILERDEVS		= 1033,
    RPMTAG_FILEMTIMES		= 1034,
    RPMTAG_FILEMD5S		= 1035,
    RPMTAG_FILELINKTOS		= 1036,
    RPMTAG_FILEFLAGS		= 1037,
/*@-enummemuse@*/
    RPMTAG_ROOT			= 1038, /*!< internal - obsolete */
/*@=enummemuse@*/
    RPMTAG_FILEUSERNAME		= 1039,
    RPMTAG_FILEGROUPNAME	= 1040,
/*@-enummemuse@*/
    RPMTAG_EXCLUDE		= 1041, /*!< internal - obsolete */
    RPMTAG_EXCLUSIVE		= 1042, /*!< internal - obsolete */
/*@=enummemuse@*/
    RPMTAG_ICON			= 1043,
    RPMTAG_SOURCERPM		= 1044,
    RPMTAG_FILEVERIFYFLAGS	= 1045,
    RPMTAG_ARCHIVESIZE		= 1046,
    RPMTAG_PROVIDENAME		= 1047,
#define	RPMTAG_PROVIDES RPMTAG_PROVIDENAME	/* backward comaptibility */
    RPMTAG_REQUIREFLAGS		= 1048,
    RPMTAG_REQUIRENAME		= 1049,
    RPMTAG_REQUIREVERSION	= 1050,
    RPMTAG_NOSOURCE		= 1051, /*!< internal */
    RPMTAG_NOPATCH		= 1052, /*!< internal */
    RPMTAG_CONFLICTFLAGS	= 1053,
    RPMTAG_CONFLICTNAME		= 1054,
    RPMTAG_CONFLICTVERSION	= 1055,
    RPMTAG_DEFAULTPREFIX	= 1056, /*!< internal - deprecated */
    RPMTAG_BUILDROOT		= 1057, /*!< internal */
    RPMTAG_INSTALLPREFIX	= 1058, /*!< internal - deprecated */
    RPMTAG_EXCLUDEARCH		= 1059,
    RPMTAG_EXCLUDEOS		= 1060,
    RPMTAG_EXCLUSIVEARCH	= 1061,
    RPMTAG_EXCLUSIVEOS		= 1062,
    RPMTAG_AUTOREQPROV		= 1063, /*!< internal */
    RPMTAG_RPMVERSION		= 1064,
    RPMTAG_TRIGGERSCRIPTS	= 1065,
    RPMTAG_TRIGGERNAME		= 1066,
    RPMTAG_TRIGGERVERSION	= 1067,
    RPMTAG_TRIGGERFLAGS		= 1068,
    RPMTAG_TRIGGERINDEX		= 1069,
    RPMTAG_VERIFYSCRIPT		= 1079,
    RPMTAG_CHANGELOGTIME	= 1080,
    RPMTAG_CHANGELOGNAME	= 1081,
    RPMTAG_CHANGELOGTEXT	= 1082,
/*@-enummemuse@*/
    RPMTAG_BROKENMD5		= 1083, /*!< internal */
/*@=enummemuse@*/
    RPMTAG_PREREQ		= 1084, /*!< internal */
    RPMTAG_PREINPROG		= 1085,
    RPMTAG_POSTINPROG		= 1086,
    RPMTAG_PREUNPROG		= 1087,
    RPMTAG_POSTUNPROG		= 1088,
    RPMTAG_BUILDARCHS		= 1089,
    RPMTAG_OBSOLETENAME		= 1090,
#define	RPMTAG_OBSOLETES RPMTAG_OBSOLETENAME	/* backward comaptibility */
    RPMTAG_VERIFYSCRIPTPROG	= 1091,
    RPMTAG_TRIGGERSCRIPTPROG	= 1092,
    RPMTAG_DOCDIR		= 1093, /*!< internal */
    RPMTAG_COOKIE		= 1094,
    RPMTAG_FILEDEVICES		= 1095,
    RPMTAG_FILEINODES		= 1096,
    RPMTAG_FILELANGS		= 1097,
    RPMTAG_PREFIXES		= 1098,
    RPMTAG_INSTPREFIXES		= 1099,
    RPMTAG_TRIGGERIN		= 1100, /*!< internal */
    RPMTAG_TRIGGERUN		= 1101, /*!< internal */
    RPMTAG_TRIGGERPOSTUN	= 1102, /*!< internal */
    RPMTAG_AUTOREQ		= 1103, /*!< internal */
    RPMTAG_AUTOPROV		= 1104, /*!< internal */
/*@-enummemuse@*/
    RPMTAG_CAPABILITY		= 1105, /*!< internal - obsolete */
/*@=enummemuse@*/
    RPMTAG_SOURCEPACKAGE	= 1106, /*!< internal */
/*@-enummemuse@*/
    RPMTAG_OLDORIGFILENAMES	= 1107, /*!< internal - obsolete */
/*@=enummemuse@*/
    RPMTAG_BUILDPREREQ		= 1108, /*!< internal */
    RPMTAG_BUILDREQUIRES	= 1109, /*!< internal */
    RPMTAG_BUILDCONFLICTS	= 1110, /*!< internal */
/*@-enummemuse@*/
    RPMTAG_BUILDMACROS		= 1111, /*!< internal */
/*@=enummemuse@*/
    RPMTAG_PROVIDEFLAGS		= 1112,
    RPMTAG_PROVIDEVERSION	= 1113,
    RPMTAG_OBSOLETEFLAGS	= 1114,
    RPMTAG_OBSOLETEVERSION	= 1115,
    RPMTAG_DIRINDEXES		= 1116,
    RPMTAG_BASENAMES		= 1117,
    RPMTAG_DIRNAMES		= 1118,
    RPMTAG_ORIGDIRINDEXES	= 1119, /*!< internal */
    RPMTAG_ORIGBASENAMES	= 1120, /*!< internal */
    RPMTAG_ORIGDIRNAMES		= 1121, /*!< internal */
    RPMTAG_OPTFLAGS		= 1122,
    RPMTAG_DISTURL		= 1123,
    RPMTAG_PAYLOADFORMAT	= 1124,
    RPMTAG_PAYLOADCOMPRESSOR	= 1125,
    RPMTAG_PAYLOADFLAGS		= 1126,
    RPMTAG_MULTILIBS		= 1127,
    RPMTAG_INSTALLTID		= 1128,
    RPMTAG_REMOVETID		= 1129,
    RPMTAG_SHA1RHN		= 1130, /*!< internal */
    RPMTAG_RHNPLATFORM		= 1131,
    RPMTAG_PLATFORM		= 1132,
/*@-enummemuse@*/
    RPMTAG_FIRSTFREE_TAG	/*!< internal */
/*@=enummemuse@*/
} rpmTag;

#define	RPMTAG_EXTERNAL_TAG		1000000

/** \ingroup signature
 * Tags found in signature header from package.
 */
enum rpmtagSignature {
    RPMSIGTAG_SIZE	= 1000,	/*!< Header+Payload size in bytes. */
/* the md5 sum was broken *twice* on big endian machines */
    RPMSIGTAG_LEMD5_1	= 1001,	/*!< Broken MD5, take 1 */
    RPMSIGTAG_PGP	= 1002,	/*!< PGP 2.6.3 signature. */
    RPMSIGTAG_LEMD5_2	= 1003,	/*!< Broken MD5, take 2 */
    RPMSIGTAG_MD5	= 1004,	/*!< MD5 signature. */
    RPMSIGTAG_GPG	= 1005, /*!< GnuPG signature. */
    RPMSIGTAG_PGP5	= 1006,	/*!< PGP5 signature @deprecated legacy. */
    RPMSIGTAG_PAYLOADSIZE = 1007,
				/*!< uncompressed payload size in bytes. */
    RPMSIGTAG_BADSHA1_1 = RPMTAG_BADSHA1_1,	/*!< Broken SHA1, take 1. */
    RPMSIGTAG_BADSHA1_2 = RPMTAG_BADSHA1_2,	/*!< Broken SHA1, take 2. */
    RPMSIGTAG_SHA1	= RPMTAG_SHA1HEADER,	/*!< sha1 header digest. */
    RPMSIGTAG_DSA	= RPMTAG_DSAHEADER,	/*!< DSA header signature. */
    RPMSIGTAG_RSA	= RPMTAG_RSAHEADER	/*!< RSA header signature. */
};

/**
 * Dependency Attributes.
 */
typedef	enum rpmsenseFlags_e {
    RPMSENSE_ANY	= 0,
/*@-enummemuse@*/
    RPMSENSE_SERIAL	= (1 << 0),	/*!< @todo Legacy. */
/*@=enummemuse@*/
    RPMSENSE_LESS	= (1 << 1),
    RPMSENSE_GREATER	= (1 << 2),
    RPMSENSE_EQUAL	= (1 << 3),
    RPMSENSE_PROVIDES	= (1 << 4), /* only used internally by builds */
    RPMSENSE_CONFLICTS	= (1 << 5), /* only used internally by builds */
    RPMSENSE_PREREQ	= (1 << 6),	/*!< @todo Legacy. */
    RPMSENSE_OBSOLETES	= (1 << 7), /* only used internally by builds */
    RPMSENSE_INTERP	= (1 << 8),	/*!< Interpreter used by scriptlet. */
    RPMSENSE_SCRIPT_PRE	= ((1 << 9)|RPMSENSE_PREREQ), /*!< %pre dependency. */
    RPMSENSE_SCRIPT_POST = ((1 << 10)|RPMSENSE_PREREQ), /*!< %post dependency. */
    RPMSENSE_SCRIPT_PREUN = ((1 << 11)|RPMSENSE_PREREQ), /*!< %preun dependency. */
    RPMSENSE_SCRIPT_POSTUN = ((1 << 12)|RPMSENSE_PREREQ), /*!< %postun dependency. */
    RPMSENSE_SCRIPT_VERIFY = (1 << 13),	/*!< %verify dependency. */
    RPMSENSE_FIND_REQUIRES = (1 << 14), /*!< find-requires generated dependency. */
    RPMSENSE_FIND_PROVIDES = (1 << 15), /*!< find-provides generated dependency. */

    RPMSENSE_TRIGGERIN	= (1 << 16),	/*!< %triggerin dependency. */
    RPMSENSE_TRIGGERUN	= (1 << 17),	/*!< %triggerun dependency. */
    RPMSENSE_TRIGGERPOSTUN = (1 << 18),	/*!< %triggerpostun dependency. */
    RPMSENSE_MULTILIB	= (1 << 19),
    RPMSENSE_SCRIPT_PREP = (1 << 20),	/*!< %prep build dependency. */
    RPMSENSE_SCRIPT_BUILD = (1 << 21),	/*!< %build build dependency. */
    RPMSENSE_SCRIPT_INSTALL = (1 << 22),/*!< %install build dependency. */
    RPMSENSE_SCRIPT_CLEAN = (1 << 23),	/*!< %clean build dependency. */
    RPMSENSE_RPMLIB	= ((1 << 24) | RPMSENSE_PREREQ), /*!< rpmlib(feature) dependency. */
/*@-enummemuse@*/
    RPMSENSE_TRIGGERPREIN = (1 << 25),	/*!< @todo Implement %triggerprein. */
/*@=enummemuse@*/

/*@-enummemuse@*/
    RPMSENSE_KEYRING	= (1 << 26)
/*@=enummemuse@*/
} rpmsenseFlags;

/** \ingroup header
 * Include calculation for 8 bytes of (magic, 0)?
 */
enum hMagic {
	HEADER_MAGIC_NO		= 0,
	HEADER_MAGIC_YES	= 1
};

/**
 * Package read return codes.
 */
typedef	enum rpmRC_e {
    RPMRC_OK		= 0,
    RPMRC_BADMAGIC	= 1,
    RPMRC_FAIL		= 2,
    RPMRC_BADSIZE	= 3,
    RPMRC_SHORTREAD	= 4
} rpmRC;

/** \ingroup header
 * The basic types of data in tags from headers.
 */
typedef enum rpmTagType_e {
#define	RPM_MIN_TYPE		0
    RPM_NULL_TYPE		=  0,
    RPM_CHAR_TYPE		=  1,
    RPM_INT8_TYPE		=  2,
    RPM_INT16_TYPE		=  3,
    RPM_INT32_TYPE		=  4,
/*    RPM_INT64_TYPE	= 5,   ---- These aren't supported (yet) */
    RPM_STRING_TYPE		=  6,
    RPM_BIN_TYPE		=  7,
    RPM_STRING_ARRAY_TYPE	=  8,
    RPM_I18NSTRING_TYPE		=  9
#define	RPM_MAX_TYPE		9
} rpmTagType;

/*
 * Teach header.c about legacy tags.
 */
#define	HEADER_OLDFILENAMES	1027
#define	HEADER_BASENAMES	1117

#define	REGION_TAG_TYPE		RPM_BIN_TYPE
#define	REGION_TAG_COUNT	sizeof(struct entryInfo)

#define	RPMLEAD_BINARY 0
#define	RPMLEAD_SOURCE 1

#define	RPMLEAD_MAGIC0 0xed
#define	RPMLEAD_MAGIC1 0xab
#define	RPMLEAD_MAGIC2 0xee
#define	RPMLEAD_MAGIC3 0xdb

#define	RPMLEAD_SIZE 96		/*!< Don't rely on sizeof(struct) */

/** \ingroup header
 * Maximum no. of bytes permitted in a header.
 */
/*@unchecked@*/
static size_t headerMaxbytes = (32*1024*1024);

/** \ingroup lead
 * The lead data structure.
 * The lead needs to be 8 byte aligned.
 * @deprecated The lead (except for signature_type) is legacy.
 * @todo Don't use any information from lead.
 */
struct rpmlead {
    unsigned char magic[4];
    unsigned char major, minor;
    short type;
    short archnum;
    char name[66];
    short osnum;
    short signature_type;	/*!< Signature header type (RPMSIG_HEADERSIG) */
/*@unused@*/ char reserved[16];	/*!< Pad to 96 bytes -- 8 byte aligned */
} ;

/** \ingroup header
 * Alignment needs (and sizeof scalars types) for internal rpm data types.
 */
/*@observer@*/ /*@unchecked@*/
static int typeSizes[] =  {
	0,	/*!< RPM_NULL_TYPE */
	1,	/*!< RPM_CHAR_TYPE */
	1,	/*!< RPM_INT8_TYPE */
	2,	/*!< RPM_INT16_TYPE */
	4,	/*!< RPM_INT32_TYPE */
	-1,	/*!< RPM_INT64_TYPE */
	-1,	/*!< RPM_STRING_TYPE */
	1,	/*!< RPM_BIN_TYPE */
	-1,	/*!< RPM_STRING_ARRAY_TYPE */
	-1	/*!< RPM_I18NSTRING_TYPE */
};

/** \ingroup header
 */
enum headerSprintfExtenstionType {
    HEADER_EXT_LAST = 0,	/*!< End of extension chain. */
    HEADER_EXT_FORMAT,		/*!< headerTagFormatFunction() extension */
    HEADER_EXT_MORE,		/*!< Chain to next table. */
    HEADER_EXT_TAG		/*!< headerTagTagFunction() extension */
};

/** \ingroup signature
 * Signature types stored in rpm lead.
 */
typedef	enum sigType_e {
    RPMSIGTYPE_NONE	= 0,	/*!< unused, legacy. */
    RPMSIGTYPE_PGP262_1024 = 1,	/*!< unused, legacy. */
/*@-enummemuse@*/
    RPMSIGTYPE_BAD	= 2,	/*!< Unknown signature type. */
/*@=enummemuse@*/
    RPMSIGTYPE_MD5	= 3,	/*!< unused, legacy. */
    RPMSIGTYPE_MD5_PGP	= 4,	/*!< unused, legacy. */
    RPMSIGTYPE_HEADERSIG= 5,	/*!< Header style signature */
    RPMSIGTYPE_DISABLE	= 6	/*!< Disable verification (debugging only) */
} sigType;

/** \ingroup header
 * HEADER_EXT_TAG format function prototype.
 * This will only ever be passed RPM_INT32_TYPE or RPM_STRING_TYPE to
 * help keep things simple.
 *
 * @param type		tag type
 * @param data		tag value
 * @param formatPrefix
 * @param padding
 * @param element
 * @return		formatted string
 */
typedef /*only@*/ char * (*headerTagFormatFunction)(int_32 type,
				const void * data, char * formatPrefix,
				int padding, int element);


/** \ingroup header
 * Associate tag names with numeric values.
 */
typedef /*@abstract@*/ struct headerTagTableEntry_s * headerTagTableEntry;
struct headerTagTableEntry_s {
/*@observer@*/ /*@null@*/ const char * name;	/*!< Tag name. */
    int val;					/*!< Tag numeric value. */
};

/** \ingroup header
 */
typedef /*@abstract@*/ struct headerIteratorS * HeaderIterator;

/** \ingroup header
 */
typedef /*@abstract@*/ /*@refcounted@*/ struct headerToken * Header;


typedef int_32 * hTYP_t;
typedef const void *	hPTR_t;
typedef int_32 *	hCNT_t;

/** \ingroup header
 * HEADER_EXT_FORMAT format function prototype.
 * This is allowed to fail, which indicates the tag doesn't exist.
 *
 * @param h		header
 * @retval type		address of tag type
 * @retval data		address of tag value pointer
 * @retval count	address of no. of data items
 * @retval freedata	address of data-was-malloc'ed indicator
 * @return		0 on success
 */
typedef int (*headerTagTagFunction) (Header h,
		/*@null@*/ /*@out@*/ hTYP_t type,
		/*@null@*/ /*@out@*/ hPTR_t * data,
		/*@null@*/ /*@out@*/ hCNT_t count,
		/*@null@*/ /*@out@*/ int * freeData);

/** \ingroup header
 * Define header tag output formats.
 */
typedef /*@abstract@*/ struct headerSprintfExtension_s * headerSprintfExtension;
struct headerSprintfExtension_s {
    enum headerSprintfExtenstionType type;	/*!< Type of extension. */
/*@observer@*/ /*@null@*/
    const char * name;				/*!< Name of extension. */
    union {
/*@observer@*/ /*@null@*/
	void * generic;				/*!< Private extension. */
	headerTagFormatFunction formatFunction; /*!< HEADER_EXT_TAG extension. */
	headerTagTagFunction tagFunction;	/*!< HEADER_EXT_FORMAT extension. */
	struct headerSprintfExtension_s * more;	/*!< Chained table extension. */
    } u;
};

/** \ingroup header
 * Create new (empty) header instance.
 * @return		header
 */
typedef
Header (*HDRnew) (void)
	/*@*/;

/** \ingroup header
 * Dereference a header instance.
 * @param h		header
 * @return		NULL always
 */
typedef
/*@null@*/ Header (*HDRfree) (/*@null@*/ /*@killref@*/ Header h)
        /*@modifies h @*/;

/** \ingroup header
 * Reference a header instance.
 * @param h		header
 * @return		referenced header instance
 */
typedef
Header (*HDRlink) (Header h)
        /*@modifies h @*/;

/** \ingroup header
 * Dereference a header instance.
 * @param h		header
 * @return		NULL always
 */
typedef
Header (*HDRunlink) (/*@killref@*/ /*@null@*/ Header h)
         /*@modifies h @*/;

/** \ingroup header
 * Sort tags in header.
 * @todo Eliminate from API.
 * @param h		header
 */
typedef
void (*HDRsort) (Header h)
        /*@modifies h @*/;

/** \ingroup header
 * Restore tags in header to original ordering.
 * @todo Eliminate from API.
 * @param h		header
 */
typedef
void (*HDRunsort) (Header h)
        /*@modifies h @*/;

/** \ingroup header
 * Return size of on-disk header representation in bytes.
 * @param h		header
 * @param magicp	include size of 8 bytes for (magic, 0)?
 * @return		size of on-disk header
 */
typedef
unsigned int (*HDRsizeof) (/*@null@*/ Header h, enum hMagic magicp)
        /*@modifies h @*/;

/** \ingroup header
 * Convert header to on-disk representation.
 * @param h		header (with pointers)
 * @return		on-disk header blob (i.e. with offsets)
 */
typedef
/*@only@*/ /*@null@*/ void * (*HDRunload) (Header h)
        /*@modifies h @*/;

/** \ingroup header
 * Convert header to on-disk representation, and then reload.
 * This is used to insure that all header data is in one chunk.
 * @param h		header (with pointers)
 * @param tag		region tag
 * @return		on-disk header (with offsets)
 */
typedef
/*@null@*/ Header (*HDRreload) (/*@only@*/ Header h, int tag)
        /*@modifies h @*/;

/** \ingroup header
 * Duplicate a header.
 * @param h		header
 * @return		new header instance
 */
typedef
Header (*HDRcopy) (Header h)
        /*@modifies h @*/;

/** \ingroup header
 * Convert header to in-memory representation.
 * @param uh		on-disk header blob (i.e. with offsets)
 * @return		header
 */
typedef
/*@null@*/ Header (*HDRload) (/*@kept@*/ void * uh)
	/*@modifies uh @*/;

/** \ingroup header
 * Make a copy and convert header to in-memory representation.
 * @param uh		on-disk header blob (i.e. with offsets)
 * @return		header
 */
typedef
/*@null@*/ Header (*HDRcopyload) (const void * uh)
	/*@*/;

/** \ingroup header
 * Read (and load) header from file handle.
 * @param fd		file handle
 * @param magicp	read (and verify) 8 bytes of (magic, 0)?
 * @return		header (or NULL on error)
 */
typedef
/*@null@*/ Header (*HDRhdrread) (FD_t fd, enum hMagic magicp)
	/*@modifies fd @*/;

/** \ingroup header
 * Write (with unload) header to file handle.
 * @param fd		file handle
 * @param h		header
 * @param magicp	prefix write with 8 bytes of (magic, 0)?
 * @return		0 on success, 1 on error
 */
typedef
int (*HDRhdrwrite) (FD_t fd, /*@null@*/ Header h, enum hMagic magicp)
	/*@globals fileSystem @*/
	/*@modifies fd, h, fileSystem @*/;

/** \ingroup header
 * Check if tag is in header.
 * @param h		header
 * @param tag		tag
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRisentry) (/*@null@*/Header h, int_32 tag)
        /*@*/;

/** \ingroup header
 * Free data allocated when retrieved from header.
 * @param h		header
 * @param data		address of data (or NULL)
 * @param type		type of data (or -1 to force free)
 * @return		NULL always
 */
typedef
/*@null@*/ void * (*HDRfreetag) (Header h,
		/*@only@*/ /*@null@*/ const void * data, rpmTagType type)
	/*@modifies data @*/;

/** \ingroup header
 * Retrieve tag value.
 * Will never return RPM_I18NSTRING_TYPE! RPM_STRING_TYPE elements with
 * RPM_I18NSTRING_TYPE equivalent entries are translated (if HEADER_I18NTABLE
 * entry is present).
 *
 * @param h		header
 * @param tag		tag
 * @retval type		address of tag value data type (or NULL)
 * @retval p		address of pointer to tag value(s) (or NULL)
 * @retval c		address of number of values (or NULL)
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRget) (Header h, int_32 tag,
			/*@null@*/ /*@out@*/ hTYP_t type,
			/*@null@*/ /*@out@*/ void ** p,
			/*@null@*/ /*@out@*/ hCNT_t c)
	/*@modifies *type, *p, *c @*/;

/** \ingroup header
 * Retrieve tag value using header internal array.
 * Get an entry using as little extra RAM as possible to return the tag value.
 * This is only an issue for RPM_STRING_ARRAY_TYPE.
 *
 * @param h		header
 * @param tag		tag
 * @retval type		address of tag value data type (or NULL)
 * @retval p		address of pointer to tag value(s) (or NULL)
 * @retval c		address of number of values (or NULL)
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRgetmin) (Header h, int_32 tag,
			/*@null@*/ /*@out@*/ hTYP_t type,
			/*@null@*/ /*@out@*/ hPTR_t * p,
			/*@null@*/ /*@out@*/ hCNT_t c)
	/*@modifies *type, *p, *c @*/;

/** \ingroup header
 * Add tag to header.
 * Duplicate tags are okay, but only defined for iteration (with the
 * exceptions noted below). While you are allowed to add i18n string
 * arrays through this function, you probably don't mean to. See
 * headerAddI18NString() instead.
 *
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRadd) (Header h, int_32 tag, int_32 type, const void * p, int_32 c)
        /*@modifies h @*/;

/** \ingroup header
 * Append element to tag array in header.
 * Appends item p to entry w/ tag and type as passed. Won't work on
 * RPM_STRING_TYPE. Any pointers into header memory returned from
 * headerGetEntryMinMemory() for this entry are invalid after this
 * call has been made!
 *
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRappend) (Header h, int_32 tag, int_32 type, const void * p, int_32 c)
        /*@modifies h @*/;

/** \ingroup header
 * Add or append element to tag array in header.
 * @todo Arg "p" should have const.
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRaddorappend) (Header h, int_32 tag, int_32 type, const void * p, int_32 c)
        /*@modifies h @*/;

/** \ingroup header
 * Add locale specific tag to header.
 * A NULL lang is interpreted as the C locale. Here are the rules:
 * \verbatim
 *	- If the tag isn't in the header, it's added with the passed string
 *	   as new value.
 *	- If the tag occurs multiple times in entry, which tag is affected
 *	   by the operation is undefined.
 *	- If the tag is in the header w/ this language, the entry is
 *	   *replaced* (like headerModifyEntry()).
 * \endverbatim
 * This function is intended to just "do the right thing". If you need
 * more fine grained control use headerAddEntry() and headerModifyEntry().
 *
 * @param h		header
 * @param tag		tag
 * @param string	tag value
 * @param lang		locale
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRaddi18n) (Header h, int_32 tag, const char * string,
                const char * lang)
        /*@modifies h @*/;

/** \ingroup header
 * Modify tag in header.
 * If there are multiple entries with this tag, the first one gets replaced.
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRmodify) (Header h, int_32 tag, int_32 type, const void * p, int_32 c)
        /*@modifies h @*/;

/** \ingroup header
 * Delete tag in header.
 * Removes all entries of type tag from the header, returns 1 if none were
 * found.
 *
 * @param h		header
 * @param tag		tag
 * @return		0 on success, 1 on failure (INCONSISTENT)
 */
typedef
int (*HDRremove) (Header h, int_32 tag)
        /*@modifies h @*/;

/*@-redef@*/	/* LCL: no clue */
/** \ingroup header
 */
typedef const char *	errmsg_t;
typedef int_32 *	hTAG_t;

/** \ingroup header
 * Return formatted output string from header tags.
 * The returned string must be free()d.
 *
 * @param h		header
 * @param fmt		format to use
 * @param tags		array of tag name/value pairs
 * @param extensions	chained table of formatting extensions.
 * @retval errmsg	error message (if any)
 * @return		formatted output string (malloc'ed)
 */
typedef
/*@only@*/ char * (*HDRhdrsprintf) (Header h, const char * fmt,
		     const struct headerTagTableEntry_s * tags,
		     const struct headerSprintfExtension_s * extensions,
		     /*@null@*/ /*@out@*/ errmsg_t * errmsg)
	/*@modifies *errmsg @*/;

/** \ingroup header
 * Duplicate tag values from one header into another.
 * @param headerFrom	source header
 * @param headerTo	destination header
 * @param tagstocopy	array of tags that are copied
 */
typedef
void (*HDRcopytags) (Header headerFrom, Header headerTo, hTAG_t tagstocopy)
	/*@modifies headerFrom, headerTo @*/;

/** \ingroup header
 * Destroy header tag iterator.
 * @param hi		header tag iterator
 * @return		NULL always
 */
typedef
HeaderIterator (*HDRfreeiter) (/*@only@*/ HeaderIterator hi)
	/*@modifies hi @*/;

/** \ingroup header
 * Create header tag iterator.
 * @param h		header
 * @return		header tag iterator
 */
typedef
HeaderIterator (*HDRinititer) (Header h)
	/*@modifies h */;

/** \ingroup header
 * Return next tag from header.
 * @param hi		header tag iterator
 * @retval tag		address of tag
 * @retval type		address of tag value data type
 * @retval p		address of pointer to tag value(s)
 * @retval c		address of number of values
 * @return		1 on success, 0 on failure
 */
typedef
int (*HDRnextiter) (HeaderIterator hi,
		/*@null@*/ /*@out@*/ hTAG_t tag,
		/*@null@*/ /*@out@*/ hTYP_t type,
		/*@null@*/ /*@out@*/ hPTR_t * p,
		/*@null@*/ /*@out@*/ hCNT_t c)
	/*@modifies hi, *tag, *type, *p, *c @*/;

/** \ingroup header
 * Header method vectors.
 */
typedef /*@abstract@*/ struct HV_s * HV_t;
struct HV_s {
    HDRnew	hdrnew;
    HDRfree	hdrfree;
    HDRlink	hdrlink;
    HDRsort	hdrsort;
    HDRunsort	hdrunsort;
    HDRsizeof	hdrsizeof;
    HDRunload	hdrunload;
    HDRreload	hdrreload;
    HDRcopy	hdrcopy;
    HDRload	hdrload;
    HDRcopyload	hdrcopyload;
    HDRhdrread	hdrread;
    HDRhdrwrite	hdrwrite;
    HDRisentry	hdrisentry;
    HDRfreetag	hdrfreetag;
    HDRget	hdrget;
    HDRgetmin	hdrgetmin;
    HDRadd	hdradd;
    HDRappend	hdrappend;
    HDRaddorappend hdraddorappend;
    HDRaddi18n	hdraddi18n;
    HDRmodify	hdrmodify;
    HDRremove	hdrremove;
    HDRhdrsprintf hdrsprintf;
    HDRcopytags	hdrcopytags;
    HDRfreeiter	hdrfreeiter;
    HDRinititer	hdrinititer;
    HDRnextiter	hdrnextiter;
    HDRunlink	hdrunlink;
/*@null@*/
    void *	hdrvecs;
/*@null@*/
    void *	hdrdata;
    int		hdrversion;
};

/** \ingroup header
 * Description of tag data.
 */
typedef /*@abstract@*/ struct entryInfo * entryInfo;
struct entryInfo {
    int_32 tag;			/*!< Tag identifier. */
    int_32 type;		/*!< Tag data type. */
    int_32 offset;		/*!< Offset into data segment (ondisk only). */
    int_32 count;		/*!< Number of tag elements. */
};

/** \ingroup header
 * A single tag from a Header.
 */
typedef /*@abstract@*/ struct indexEntry * indexEntry;
struct indexEntry {
    struct entryInfo info;	/*!< Description of tag data. */
/*@owned@*/ void * data; 	/*!< Location of tag data. */
    int length;			/*!< No. bytes of data. */
    int rdlen;			/*!< No. bytes of data in region. */
};

/** \ingroup header
 * The Header data structure.
 */
struct headerToken {
/*@unused@*/ struct HV_s hv;	/*!< Header public methods. */
    void * blob;		/*!< Header region blob. */
/*@owned@*/ indexEntry index;	/*!< Array of tags. */
    int indexUsed;		/*!< Current size of tag array. */
    int indexAlloced;		/*!< Allocated size of tag array. */
    int flags;
#define	HEADERFLAG_SORTED	(1 << 0) /*!< Are header entries sorted? */
#define	HEADERFLAG_ALLOCATED	(1 << 1) /*!< Is 1st header region allocated? */
#define	HEADERFLAG_LEGACY	(1 << 2) /*!< Header came from legacy source? */
/*@refs@*/ int nrefs;	/*!< Reference count. */
};

/**
 * Header tag iterator data structure.
 */
struct headerIteratorS {
/*@unused@*/ Header h;		/*!< Header being iterated. */
/*@unused@*/ int next_index;	/*!< Next tag index. */
};

/*@}*/
/* ==================================================================== */
/** \name RPMTS */
/*@{*/
/**
 * Prototype for headerFreeData() vector.
 * @param data		address of data (or NULL)
 * @param type		type of data (or -1 to force free)
 * @return		NULL always
 */
typedef /*@null@*/
    void * (*HFD_t) (/*@only@*/ /*@null@*/ const void * data, rpmTagType type)
	/*@modifies data @*/;

/**
 * Prototype for headerAddEntry() vector.
 * Duplicate tags are okay, but only defined for iteration (with the
 * exceptions noted below). While you are allowed to add i18n string
 * arrays through this function, you probably don't mean to. See
 * headerAddI18NString() instead.
 *
 * @param h             header
 * @param tag           tag
 * @param type          tag value data type
 * @param p             pointer to tag value(s)
 * @param c             number of values
 * @return              1 on success, 0 on failure
 */
typedef int (*HAE_t) (Header h, rpmTag tag, rpmTagType type,
			const void * p, int_32 c)
	/*@modifies h @*/;

/**
 * Prototype for headerGetEntry() vector.
 * Will never return RPM_I18NSTRING_TYPE! RPM_STRING_TYPE elements with
 * RPM_I18NSTRING_TYPE equivalent entries are translated (if HEADER_I18NTABLE
 * entry is present).
 *
 * @param h		header
 * @param tag		tag
 * @retval type		address of tag value data type (or NULL)
 * @retval p		address of pointer to tag value(s) (or NULL)
 * @retval c		address of number of values (or NULL)
 * @return		1 on success, 0 on failure
 */
typedef int (*HGE_t) (Header h, rpmTag tag,
			/*@null@*/ /*@out@*/ rpmTagType * type,
			/*@null@*/ /*@out@*/ void ** p,
			/*@null@*/ /*@out@*/ int_32 * c)
	/*@modifies *type, *p, *c @*/;

/**
 * Prototype for headerRemoveEntry() vector.
 * Delete tag in header.
 * Removes all entries of type tag from the header, returns 1 if none were
 * found.
 *
 * @param h		header
 * @param tag		tag
 * @return		0 on success, 1 on failure (INCONSISTENT)
 */
typedef int (*HRE_t) (Header h, int_32 tag)
	/*@modifies h @*/;

#define	ENTRY_IS_REGION(_e) \
	(((_e)->info.tag >= HEADER_IMAGE) && ((_e)->info.tag < HEADER_REGIONS))


#define	alloca_strdup(_s)	strcpy(alloca(strlen(_s)+1), (_s))

/**
 * Wrapper to free(3), hides const compilation noise, permit NULL, return NULL.
 * @param p		memory to free
 * @return		NULL always
 */
/*@unused@*/ static /*@null@*/ void *
_free(/*@only@*/ /*@null@*/ /*@out@*/ const void * p) /*@modifies *p @*/
{
    if (p != NULL)	free((void *)p);
    return NULL;
}

/**
 */
static int indexCmp(const void * avp, const void * bvp)	/*@*/
{
    /*@-castexpose@*/
    indexEntry ap = (indexEntry) avp, bp = (indexEntry) bvp;
    /*@=castexpose@*/
    return (ap->info.tag - bp->info.tag);
}

/** \ingroup header
 * Sort tags in header.
 * @param h		header
 */
static
void headerSort(Header h)
	/*@modifies h @*/
{
    if (!(h->flags & HEADERFLAG_SORTED)) {
	qsort(h->index, h->indexUsed, sizeof(*h->index), indexCmp);
	h->flags |= HEADERFLAG_SORTED;
    }
}

/**
 * Remove occurences of trailing character from string.
 * @param s		string
 * @param c		character to strip
 * @return 		string
 */
/*@unused@*/ static inline
/*@only@*/ char * stripTrailingChar(/*@only@*/ char * s, char c)
	/*@modifies *s */
{
    char * t;
    for (t = s + strlen(s) - 1; *t == c && t >= s; t--)
	*t = '\0';
    return s;
}

/** \ingroup header
 * Free data allocated when retrieved from header.
 * @deprecated Use headerFreeTag() instead.
 * @todo Remove from API.
 *
 * @param data		address of data (or NULL)
 * @param type		type of data (or -1 to force free)
 * @return		NULL always
 */
/*@unused@*/ static /*@null@*/
void * headerFreeData( /*@only@*/ /*@null@*/ const void * data, rpmTagType type)
	/*@modifies data @*/
{
    if (data) {
	/*@-branchstate@*/
	if (type == -1 ||
	    type == RPM_STRING_ARRAY_TYPE ||
	    type == RPM_I18NSTRING_TYPE ||
	    type == RPM_BIN_TYPE)
	  free((void *)data);
	/*@=branchstate@*/
    }
    return NULL;
}


/**
 * Return length of entry data.
 * @todo Remove sanity check exit's.
 * @param type		entry data type
 * @param p		entry data
 * @param count		entry item count
 * @param onDisk	data is concatenated strings (with NUL's))?
 * @return		no. bytes in data
 */
/*@mayexit@*/
static int dataLength(int_32 type, hPTR_t p, int_32 count, int onDisk)
	/*@*/
{
    int length = 0;

    switch (type) {
    case RPM_STRING_TYPE:
	if (count == 1) {	/* Special case -- p is just the string */
	    length = strlen(p) + 1;
	    break;
	}
        /* This should not be allowed */
	/*@-modfilesys@*/
	/*@=modfilesys@*/
	exit(EXIT_FAILURE);
	/*@notreached@*/ break;

    case RPM_STRING_ARRAY_TYPE:
    case RPM_I18NSTRING_TYPE:
    {	int i;

	/* This is like RPM_STRING_TYPE, except it's *always* an array */
	/* Compute sum of length of all strings, including null terminators */
	i = count;

	if (onDisk) {
	    const char * chptr = p;
	    int thisLen;

	    while (i--) {
		thisLen = strlen(chptr) + 1;
		length += thisLen;
		chptr += thisLen;
	    }
	} else {
	    const char ** src = (const char **)p;
	    while (i--) {
		/* add one for null termination */
		length += strlen(*src++) + 1;
	    }
	}
    }	break;

    default:
	if (typeSizes[type] != -1) {
	    length = typeSizes[type] * count;
	    break;
	}
	/*@-modfilesys@*/
	/*@=modfilesys@*/
	exit(EXIT_FAILURE);
	/*@notreached@*/ break;
    }

    return length;
}

/**
 */
static void copyData(int_32 type, /*@out@*/ void * dstPtr, const void * srcPtr,
		int_32 c, int dataLength)
	/*@modifies *dstPtr @*/
{
    const char ** src;
    char * dst;
    int i;

    switch (type) {
    case RPM_STRING_ARRAY_TYPE:
    case RPM_I18NSTRING_TYPE:
	/* Otherwise, p is char** */
	i = c;
	src = (const char **) srcPtr;
	dst = dstPtr;
	while (i--) {
	    if (*src) {
		int len = strlen(*src) + 1;
		memcpy(dst, *src, len);
		dst += len;
	    }
	    src++;
	}
	break;

    default:
	memmove(dstPtr, srcPtr, dataLength);
	break;
    }
}

/**
 * Return (malloc'ed) copy of entry data.
 * @param type		entry data type
 * @param p		entry data
 * @param c		entry item count
 * @retval lengthPtr	no. bytes in returned data
 * @return 		(malloc'ed) copy of entry data
 */
static void * grabData(int_32 type, hPTR_t p, int_32 c,
		/*@out@*/ int * lengthPtr)
	/*@modifies *lengthPtr @*/
{
    int length = dataLength(type, p, c, 0);
    void * data = malloc(length);

    copyData(type, data, p, c, length);

    if (lengthPtr)
	*lengthPtr = length;
    return data;
}


#define	INDEX_MALLOC_SIZE	8

/** \ingroup header
 * Add tag to header.
 * Duplicate tags are okay, but only defined for iteration (with the
 * exceptions noted below). While you are allowed to add i18n string
 * arrays through this function, you probably don't mean to. See
 * headerAddI18NString() instead.
 *
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
static
int headerAddEntry(Header h, int_32 tag, int_32 type, const void * p, int_32 c)
	/*@modifies h @*/
{
    indexEntry entry;

    /* Count must always be >= 1 for headerAddEntry. */
    if (c <= 0)
	return 0;

    /* Allocate more index space if necessary */
    if (h->indexUsed == h->indexAlloced) {
	h->indexAlloced += INDEX_MALLOC_SIZE;
	h->index = realloc(h->index, h->indexAlloced * sizeof(*h->index));
    }

    /* Fill in the index */
    entry = h->index + h->indexUsed;
    entry->info.tag = tag;
    entry->info.type = type;
    entry->info.count = c;
    entry->info.offset = 0;
    entry->data = grabData(type, p, c, &entry->length);

    if (h->indexUsed > 0 && tag < h->index[h->indexUsed-1].info.tag)
	h->flags &= ~HEADERFLAG_SORTED;
    h->indexUsed++;

    return 1;
}

/** \ingroup header
 * Dereference a header instance.
 * @param h		header
 * @return		NULL always
 */
static /*@null@*/
Header headerFree(/*@killref@*/ /*@null@*/ Header h);
	/*@modifies h @*/


/** \ingroup header
 * Destroy header tag iterator.
 * @param hi		header tag iterator
 * @return		NULL always
 */
static /*@null@*/
HeaderIterator headerFreeIterator(/*@only@*/ HeaderIterator hi)
	/*@modifies hi @*/
{
    hi->h = headerFree(hi->h);
    hi = _free(hi);
    return hi;
}

/**
 * Find matching (tag,type) entry in header.
 * @param h		header
 * @param tag		entry tag
 * @param type		entry type
 * @return 		header entry
 */
static /*@null@*/
indexEntry findEntry(/*@null@*/ Header h, int_32 tag, int_32 type)
	/*@modifies h @*/
{
    indexEntry entry, entry2, last;
    struct indexEntry key;

    if (h == NULL) return NULL;
    if (!(h->flags & HEADERFLAG_SORTED)) headerSort(h);

    key.info.tag = tag;

    entry2 = entry =
	bsearch(&key, h->index, h->indexUsed, sizeof(*h->index), indexCmp);
    if (entry == NULL)
	return NULL;

    if (type == RPM_NULL_TYPE)
	return entry;

    /* look backwards */
    while (entry->info.tag == tag && entry->info.type != type &&
	   entry > h->index) entry--;

    if (entry->info.tag == tag && entry->info.type == type)
	return entry;

    last = h->index + h->indexUsed;
    /*@-usereleased@*/ /* FIX: entry2 = entry. Code looks bogus as well. */
    while (entry2->info.tag == tag && entry2->info.type != type &&
	   entry2 < last) entry2++;
    /*@=usereleased@*/

    if (entry->info.tag == tag && entry->info.type == type)
	return entry;

    return NULL;
}

static int regionSwab(/*@null@*/ indexEntry entry, int il, int dl,
		      entryInfo pe, char * dataStart, int regionid);
	/*@modifies *entry, *dataStart @*/

/** \ingroup header
 * Retrieve data from header entry.
 * @todo Permit retrieval of regions other than HEADER_IMUTABLE.
 * @param entry		header entry
 * @retval type		address of type (or NULL)
 * @retval p		address of data (or NULL)
 * @retval c		address of count (or NULL)
 * @param minMem	string pointers refer to header memory?
 * @return		1 on success, otherwise error.
 */
static int copyEntry(const indexEntry entry,
		/*@null@*/ /*@out@*/ hTYP_t type,
		/*@null@*/ /*@out@*/ hPTR_t * p,
		/*@null@*/ /*@out@*/ hCNT_t c,
		int minMem)
	/*@modifies *type, *p, *c @*/
{
    int_32 count = entry->info.count;
    int rc = 1;		/* XXX 1 on success. */

    if (p)
    switch (entry->info.type) {
    case RPM_BIN_TYPE:
	/*
	 * XXX This only works for
	 * XXX 	"sealed" HEADER_IMMUTABLE/HEADER_SIGNATURES/HEADER_IMAGE.
	 * XXX This will *not* work for unsealed legacy HEADER_IMAGE (i.e.
	 * XXX a legacy header freshly read, but not yet unloaded to the rpmdb).
	 */
	if (ENTRY_IS_REGION(entry)) {
	    int_32 * ei = ((int_32 *)entry->data) - 2;
	    /*@-castexpose@*/
	    entryInfo pe = (entryInfo) (ei + 2);
	    /*@=castexpose@*/
	    char * dataStart = (char *) (pe + ntohl(ei[0]));
	    int_32 rdl = -entry->info.offset;	/* negative offset */
	    int_32 ril = rdl/sizeof(*pe);

	    /*@-sizeoftype@*/
	    rdl = entry->rdlen;
	    count = 2 * sizeof(*ei) + (ril * sizeof(*pe)) + rdl;
	    if (entry->info.tag == HEADER_IMAGE) {
		ril -= 1;
		pe += 1;
	    } else {
		count += REGION_TAG_COUNT;
		rdl += REGION_TAG_COUNT;
	    }

	    *p = malloc(count);
	    ei = (int_32 *) *p;
	    ei[0] = htonl(ril);
	    ei[1] = htonl(rdl);

	    /*@-castexpose@*/
	    pe = (entryInfo) memcpy(ei + 2, pe, (ril * sizeof(*pe)));
	    /*@=castexpose@*/

	    dataStart = (char *) memcpy(pe + ril, dataStart, rdl);
	    /*@=sizeoftype@*/

	    rc = regionSwab(NULL, ril, 0, pe, dataStart, 0);
	    /* XXX 1 on success. */
	    rc = (rc < 0) ? 0 : 1;
	} else {
	    count = entry->length;
	    *p = (!minMem
		  ? memcpy(malloc(count), entry->data, count)
		  : entry->data);
	}
	break;
    case RPM_STRING_TYPE:
	if (count == 1) {
	    *p = entry->data;
	    break;
	}
	/*@fallthrough@*/
    case RPM_STRING_ARRAY_TYPE:
    case RPM_I18NSTRING_TYPE:
    {	const char ** ptrEntry;
	/*@-sizeoftype@*/
	int tableSize = count * sizeof(char *);
	/*@=sizeoftype@*/
	char * t;
	int i;

	/*@-mods@*/
	if (minMem) {
	    *p = malloc(tableSize);
	    ptrEntry = (const char **) *p;
	    t = entry->data;
	} else {
	    t = malloc(tableSize + entry->length);
	    *p = (void *)t;
	    ptrEntry = (const char **) *p;
	    t += tableSize;
	    memcpy(t, entry->data, entry->length);
	}
	/*@=mods@*/
	for (i = 0; i < count; i++) {
	    *ptrEntry++ = t;
	    t = strchr(t, 0);
	    t++;
	}
    }	break;

    default:
	*p = entry->data;
	break;
    }
    if (type) *type = entry->info.type;
    if (c) *c = count;
    return rc;
}

#define	ENTRY_IN_REGION(_e)	((_e)->info.offset < 0)

/** \ingroup header
 * Append element to tag array in header.
 * Appends item p to entry w/ tag and type as passed. Won't work on
 * RPM_STRING_TYPE. Any pointers into header memory returned from
 * headerGetEntryMinMemory() for this entry are invalid after this
 * call has been made!
 *
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
static
int headerAppendEntry(Header h, int_32 tag, int_32 type,
		const void * p, int_32 c)
	/*@modifies h @*/
{
    indexEntry entry;
    int length;

    /* First find the tag */
    entry = findEntry(h, tag, type);
    if (!entry)
	return 0;

    if (type == RPM_STRING_TYPE || type == RPM_I18NSTRING_TYPE) {
	/* we can't do this */
	return 0;
    }

    length = dataLength(type, p, c, 0);

    if (ENTRY_IN_REGION(entry)) {
	char * t = malloc(entry->length + length);
	memcpy(t, entry->data, entry->length);
	entry->data = t;
	entry->info.offset = 0;
    } else
	entry->data = realloc(entry->data, entry->length + length);

    copyData(type, ((char *) entry->data) + entry->length, p, c, length);

    entry->length += length;

    entry->info.count += c;

    return 1;
}

/** \ingroup header
 * Add or append element to tag array in header.
 * @todo Arg "p" should have const.
 * @param h		header
 * @param tag		tag
 * @param type		tag value data type
 * @param p		pointer to tag value(s)
 * @param c		number of values
 * @return		1 on success, 0 on failure
 */
static
int headerAddOrAppendEntry(Header h, int_32 tag, int_32 type,
		const void * p, int_32 c)
	/*@modifies h @*/
{
    return (findEntry(h, tag, type)
	? headerAppendEntry(h, tag, type, p, c)
	: headerAddEntry(h, tag, type, p, c));
}

/**
 * Does locale match entry in header i18n table?
 *
 * \verbatim
 * The range [l,le) contains the next locale to match:
 *    ll[_CC][.EEEEE][@dddd]
 * where
 *    ll	ISO language code (in lowercase).
 *    CC	(optional) ISO coutnry code (in uppercase).
 *    EEEEE	(optional) encoding (not really standardized).
 *    dddd	(optional) dialect.
 * \endverbatim
 *
 * @param td		header i18n table data, NUL terminated
 * @param l		start of locale	to match
 * @param le		end of locale to match
 * @return		1 on match, 0 on no match
 */
static int headerMatchLocale(const char *td, const char *l, const char *le)
	/*@*/
{
    const char *fe;


#if 0
  { const char *s, *ll, *CC, *EE, *dd;
    char *lbuf, *t.

    /* Copy the buffer and parse out components on the fly. */
    lbuf = alloca(le - l + 1);
    for (s = l, ll = t = lbuf; *s; s++, t++) {
	switch (*s) {
	case '_':
	    *t = '\0';
	    CC = t + 1;
	    break;
	case '.':
	    *t = '\0';
	    EE = t + 1;
	    break;
	case '@':
	    *t = '\0';
	    dd = t + 1;
	    break;
	default:
	    *t = *s;
	    break;
	}
    }

    if (ll)	/* ISO language should be lower case */
	for (t = ll; *t; t++)	*t = tolower(*t);
    if (CC)	/* ISO country code should be upper case */
	for (t = CC; *t; t++)	*t = toupper(*t);

    /* There are a total of 16 cases to attempt to match. */
  }
#endif

    /* First try a complete match. */
    if (strlen(td) == (le-l) && !strncmp(td, l, (le - l)))
	return 1;

    /* Next, try stripping optional dialect and matching.  */
    for (fe = l; fe < le && *fe != '@'; fe++)
	{};
    if (fe < le && !strncmp(td, l, (fe - l)))
	return 1;

    /* Next, try stripping optional codeset and matching.  */
    for (fe = l; fe < le && *fe != '.'; fe++)
	{};
    if (fe < le && !strncmp(td, l, (fe - l)))
	return 1;

    /* Finally, try stripping optional country code and matching. */
    for (fe = l; fe < le && *fe != '_'; fe++)
	{};
    if (fe < le && !strncmp(td, l, (fe - l)))
	return 1;

    return 0;
}

/**
 * Return i18n string from header that matches locale.
 * @param h		header
 * @param entry		i18n string data
 * @return		matching i18n string (or 1st string if no match)
 */
/*@dependent@*/ /*@exposed@*/ static char *
headerFindI18NString(Header h, indexEntry entry)
	/*@*/
{
    const char *lang, *l, *le;
    indexEntry table;

    /* XXX Drepper sez' this is the order. */
    if ((lang = getenv("LANGUAGE")) == NULL &&
	(lang = getenv("LC_ALL")) == NULL &&
        (lang = getenv("LC_MESSAGES")) == NULL &&
	(lang = getenv("LANG")) == NULL)
	    return entry->data;

    /*@-mods@*/
    if ((table = findEntry(h, HEADER_I18NTABLE, RPM_STRING_ARRAY_TYPE)) == NULL)
	return entry->data;
    /*@=mods@*/

    for (l = lang; *l != '\0'; l = le) {
	const char *td;
	char *ed;
	int langNum;

	while (*l && *l == ':')			/* skip leading colons */
	    l++;
	if (*l == '\0')
	    break;
	for (le = l; *le && *le != ':'; le++)	/* find end of this locale */
	    {};

	/* For each entry in the header ... */
	for (langNum = 0, td = table->data, ed = entry->data;
	     langNum < entry->info.count;
	     langNum++, td += strlen(td) + 1, ed += strlen(ed) + 1) {

		if (headerMatchLocale(td, l, le))
		    return ed;

	}
    }

    return entry->data;
}

/**
 * Retrieve tag data from header.
 * @param h		header
 * @param tag		tag to retrieve
 * @retval type		address of type (or NULL)
 * @retval p		address of data (or NULL)
 * @retval c		address of count (or NULL)
 * @param minMem	string pointers reference header memory?
 * @return		1 on success, 0 on not found
 */
static int intGetEntry(Header h, int_32 tag,
		/*@null@*/ /*@out@*/ hTAG_t type,
		/*@null@*/ /*@out@*/ hPTR_t * p,
		/*@null@*/ /*@out@*/ hCNT_t c,
		int minMem)
	/*@modifies *type, *p, *c @*/
{
    indexEntry entry;
    int rc;

    /* First find the tag */
    /*@-mods@*/		/*@ FIX: h modified by sort. */
    entry = findEntry(h, tag, RPM_NULL_TYPE);
    /*@mods@*/
    if (entry == NULL) {
	if (type) type = 0;
	if (p) *p = NULL;
	if (c) *c = 0;
	return 0;
    }

    switch (entry->info.type) {
    case RPM_I18NSTRING_TYPE:
	rc = 1;
	if (type) *type = RPM_STRING_TYPE;
	if (c) *c = 1;
	/*@-dependenttrans@*/
	if (p) *p = headerFindI18NString(h, entry);
	/*@=dependenttrans@*/
	break;
    default:
	rc = copyEntry(entry, type, p, c, minMem);
	break;
    }

    /* XXX 1 on success */
    return ((rc == 1) ? 1 : 0);
}

/** \ingroup header
 * Retrieve tag value.
 * Will never return RPM_I18NSTRING_TYPE! RPM_STRING_TYPE elements with
 * RPM_I18NSTRING_TYPE equivalent entries are translated (if HEADER_I18NTABLE
 * entry is present).
 *
 * @param h		header
 * @param tag		tag
 * @retval type		address of tag value data type (or NULL)
 * @retval p		address of pointer to tag value(s) (or NULL)
 * @retval c		address of number of values (or NULL)
 * @return		1 on success, 0 on failure
 */
static
int headerGetEntry(Header h, int_32 tag,
			/*@null@*/ /*@out@*/ hTYP_t type,
			/*@null@*/ /*@out@*/ void ** p,
			/*@null@*/ /*@out@*/ hCNT_t c)
	/*@modifies *type, *p, *c @*/
{
    return intGetEntry(h, tag, type, (hPTR_t *)p, c, 0);
}

/** \ingroup header
 */
/*@observer@*/ /*@unchecked@*/
static unsigned char header_magic[8] = {
	0x8e, 0xad, 0xe8, 0x01, 0x00, 0x00, 0x00, 0x00
};

/** \ingroup header
 * Return size of on-disk header representation in bytes.
 * @param h		header
 * @param magicp	include size of 8 bytes for (magic, 0)?
 * @return		size of on-disk header
 */
static
unsigned int headerSizeof(/*@null@*/ Header h, enum hMagic magicp)
	/*@modifies h @*/
{
    indexEntry entry;
    unsigned int size = 0;
    unsigned int pad = 0;
    int i;

    if (h == NULL)
	return size;

    headerSort(h);

    switch (magicp) {
    case HEADER_MAGIC_YES:
	size += sizeof(header_magic);
	break;
    case HEADER_MAGIC_NO:
	break;
    }

    /*@-sizeoftype@*/
    size += 2 * sizeof(int_32);	/* count of index entries */
    /*@=sizeoftype@*/

    for (i = 0, entry = h->index; i < h->indexUsed; i++, entry++) {
	unsigned diff;
	int_32 type;

	/* Regions go in as is ... */
        if (ENTRY_IS_REGION(entry)) {
	    size += entry->length;
	    /* XXX Legacy regions do not include the region tag and data. */
	    /*@-sizeoftype@*/
	    if (i == 0 && (h->flags & HEADERFLAG_LEGACY))
		size += sizeof(struct entryInfo) + entry->info.count;
	    /*@=sizeoftype@*/
	    continue;
        }

	/* ... and region elements are skipped. */
	if (entry->info.offset < 0)
	    continue;

	/* Alignment */
	type = entry->info.type;
	if (typeSizes[type] > 1) {
	    diff = typeSizes[type] - (size % typeSizes[type]);
	    if (diff != typeSizes[type]) {
		size += diff;
		pad += diff;
	    }
	}

	/*@-sizeoftype@*/
	size += sizeof(struct entryInfo) + entry->length;
	/*@=sizeoftype@*/
    }

    return size;
}

/** \ingroup header
 * Reference a header instance.
 * @param h		header
 * @return		referenced header instance
 */
static
Header headerLink(Header h)
	/*@modifies h @*/
{
    if (h != NULL) h->nrefs++;
    /*@-refcounttrans -nullret @*/
    return h;
    /*@=refcounttrans =nullret @*/
}

/** \ingroup header
 * Create header tag iterator.
 * @param h		header
 * @return		header tag iterator
 */
static
HeaderIterator headerInitIterator(Header h)
	/*@modifies h */
{
    HeaderIterator hi = malloc(sizeof(*hi));

    headerSort(h);

    hi->h = headerLink(h);
    hi->next_index = 0;
    return hi;
}

/** \ingroup header
 * Check if tag is in header.
 * @param h		header
 * @param tag		tag
 * @return		1 on success, 0 on failure
 */
static
int headerIsEntry(/*@null@*/Header h, int_32 tag)
	/*@*/
{
    /*@-mods@*/		/*@ FIX: h modified by sort. */
    return (findEntry(h, tag, RPM_NULL_TYPE) ? 1 : 0);
    /*@=mods@*/	
}

/** \ingroup header
 * Return next tag from header.
 * @param hi		header tag iterator
 * @retval tag		address of tag
 * @retval type		address of tag value data type
 * @retval p		address of pointer to tag value(s)
 * @retval c		address of number of values
 * @return		1 on success, 0 on failure
 */
static
int headerNextIterator(HeaderIterator hi,
		/*@null@*/ /*@out@*/ hTAG_t tag,
		/*@null@*/ /*@out@*/ hTYP_t type,
		/*@null@*/ /*@out@*/ hPTR_t * p,
		/*@null@*/ /*@out@*/ hCNT_t c,
		       int do_copy)
	/*@modifies hi, *tag, *type, *p, *c @*/
{
    Header h = hi->h;
    int slot = hi->next_index;
    indexEntry entry = NULL;
    int rc;

    for (slot = hi->next_index; slot < h->indexUsed; slot++) {
	entry = h->index + slot;
	if (!ENTRY_IS_REGION(entry))
	    break;
    }
    hi->next_index = slot;
    if (entry == NULL || slot >= h->indexUsed)
	return 0;
    /*@-noeffect@*/	/* LCL: no clue */
    hi->next_index++;
    /*@=noeffect@*/

    if (tag)
	*tag = entry->info.tag;

    rc = copyEntry(entry, type, p, c, do_copy);

    /* XXX 1 on success */
    return ((rc == 1) ? 1 : 0);
}

/** \ingroup header
 * Dereference a header instance.
 * @param h		header
 * @return		NULL always
 */
static /*@null@*/
Header headerUnlink(/*@killref@*/ /*@null@*/ Header h)
	/*@modifies h @*/
{
    if (h != NULL) h->nrefs--;
    return NULL;
}

/** \ingroup header
 * Dereference a header instance.
 * @param h		header
 * @return		NULL always
 */
static /*@null@*/
Header headerFree(/*@killref@*/ /*@null@*/ Header h)
	/*@modifies h @*/
{
    (void) headerUnlink(h);

    /*@-usereleased@*/
    if (h == NULL || h->nrefs > 0)
	return NULL;	/* XXX return previous header? */

    if (h->index) {
	indexEntry entry = h->index;
	int i;
	for (i = 0; i < h->indexUsed; i++, entry++) {
	    if ((h->flags & HEADERFLAG_ALLOCATED) && ENTRY_IS_REGION(entry)) {
		if (entry->length > 0) {
		    int_32 * ei = entry->data;
		    if ((ei - 2) == h->blob) h->blob = _free(h->blob);
		    entry->data = NULL;
		}
	    } else if (!ENTRY_IN_REGION(entry)) {
		entry->data = _free(entry->data);
	    }
	    entry->data = NULL;
	}
	h->index = _free(h->index);
    }

    /*@-refcounttrans@*/ h = _free(h); /*@=refcounttrans@*/
    return h;
    /*@=usereleased@*/
}

/**
 * Sanity check on no. of tags.
 * This check imposes a limit of 65K tags, more than enough.
 */
#define hdrchkTags(_ntags)	((_ntags) & 0xffff0000)

/**
 * Sanity check on data size and/or offset.
 * This check imposes a limit of 16Mb, more than enough.
 */
#define hdrchkData(_nbytes)	((_nbytes) & 0xff000000)



/** \ingroup header
 * Swap int_32 and int_16 arrays within header region.
 *
 * This code is way more twisty than I would like.
 *
 * A bug with RPM_I18NSTRING_TYPE in rpm-2.5.x (fixed in August 1998)
 * causes the offset and length of elements in a header region to disagree
 * regarding the total length of the region data.
 *
 * The "fix" is to compute the size using both offset and length and
 * return the larger of the two numbers as the size of the region.
 * Kinda like computing left and right Riemann sums of the data elements
 * to determine the size of a data structure, go figger :-).
 *
 * There's one other twist if a header region tag is in the set to be swabbed,
 * as the data for a header region is located after all other tag data.
 *
 * @param entry		header entry
 * @param il		no. of entries
 * @param dl		start no. bytes of data
 * @param pe		header physical entry pointer (swapped)
 * @param dataStart	header data
 * @param regionid	region offset
 * @return		no. bytes of data in region, -1 on error
 */
static int regionSwab(/*@null@*/ indexEntry entry, int il, int dl,
		entryInfo pe, char * dataStart, int regionid)
	/*@modifies *entry, *dataStart @*/
{
    char * tprev = NULL;
    char * t = NULL;
    int tdel, tl = dl;
    struct indexEntry ieprev;

    memset(&ieprev, 0, sizeof(ieprev));
    for (; il > 0; il--, pe++) {
	struct indexEntry ie;
	int_32 type;

	ie.info.tag = ntohl(pe->tag);
	ie.info.type = ntohl(pe->type);
	if (ie.info.type < RPM_MIN_TYPE || ie.info.type > RPM_MAX_TYPE)
	    return -1;
	ie.info.count = ntohl(pe->count);
	ie.info.offset = ntohl(pe->offset);
	ie.data = t = dataStart + ie.info.offset;
	ie.length = dataLength(ie.info.type, ie.data, ie.info.count, 1);
	ie.rdlen = 0;

	if (entry) {
	    ie.info.offset = regionid;
	    *entry = ie;	/* structure assignment */
	    entry++;
	}

	/* Alignment */
	type = ie.info.type;
	if (typeSizes[type] > 1) {
	    unsigned diff;
	    diff = typeSizes[type] - (dl % typeSizes[type]);
	    if (diff != typeSizes[type]) {
		dl += diff;
		if (ieprev.info.type == RPM_I18NSTRING_TYPE)
		    ieprev.length += diff;
	    }
	}
	tdel = (tprev ? (t - tprev) : 0);
	if (ieprev.info.type == RPM_I18NSTRING_TYPE)
	    tdel = ieprev.length;

	if (ie.info.tag >= HEADER_I18NTABLE) {
	    tprev = t;
	} else {
	    tprev = dataStart;
	    /* XXX HEADER_IMAGE tags don't include region sub-tag. */
	    /*@-sizeoftype@*/
	    if (ie.info.tag == HEADER_IMAGE)
		tprev -= REGION_TAG_COUNT;
	    /*@=sizeoftype@*/
	}

	/* Perform endian conversions */
	switch (ntohl(pe->type)) {
	case RPM_INT32_TYPE:
	{   int_32 * it = (int_32 *)t;
	    for (; ie.info.count > 0; ie.info.count--, it += 1)
		*it = htonl(*it);
	    t = (char *) it;
	}   /*@switchbreak@*/ break;
	case RPM_INT16_TYPE:
	{   unsigned short * it = (unsigned short *) t;
	    for (; ie.info.count > 0; ie.info.count--, it += 1)
		*it = htons(*it);
	    t = (char *) it;
	}   /*@switchbreak@*/ break;
	default:
	    t += ie.length;
	    /*@switchbreak@*/ break;
	}

	dl += ie.length;
	tl += tdel;
	ieprev = ie;	/* structure assignment */

    }
    tdel = (tprev ? (t - tprev) : 0);
    tl += tdel;

    /* XXX
     * There are two hacks here:
     *	1) tl is 16b (i.e. REGION_TAG_COUNT) short while doing headerReload().
     *	2) the 8/98 rpm bug with inserting i18n tags needs to use tl, not dl.
     */
    /*@-sizeoftype@*/
    if (tl+REGION_TAG_COUNT == dl)
	tl += REGION_TAG_COUNT;
    /*@=sizeoftype@*/

    return dl;
}

static
int headerRemoveEntry(Header h, int_32 tag);

/** \ingroup header
 * Convert header to in-memory representation.
 * @param uh		on-disk header blob (i.e. with offsets)
 * @return		header
 */
static /*@null@*/
Header headerLoad(/*@kept@*/ void * uh)
	/*@modifies uh @*/
{
    int_32 * ei = (int_32 *) uh;
    int_32 il = ntohl(ei[0]);		/* index length */
    int_32 dl = ntohl(ei[1]);		/* data length */
    /*@-sizeoftype@*/
    size_t pvlen = sizeof(il) + sizeof(dl) +
               (il * sizeof(struct entryInfo)) + dl;
    /*@=sizeoftype@*/
    void * pv = uh;
    Header h = NULL;
    entryInfo pe;
    char * dataStart;
    indexEntry entry;
    int rdlen;
    int i;

    /* Sanity checks on header intro. */
    if (hdrchkTags(il) || hdrchkData(dl))
	goto errxit;

    ei = (int_32 *) pv;
    /*@-castexpose@*/
    pe = (entryInfo) &ei[2];
    /*@=castexpose@*/
    dataStart = (char *) (pe + il);

    h = calloc(1, sizeof(*h));
    memset(h, 0, sizeof(*h));
    /*@-assignexpose@*/
    /*@=assignexpose@*/
    /*@-assignexpose -kepttrans@*/
    h->blob = uh;
    /*@=assignexpose =kepttrans@*/
    h->indexAlloced = il + 1;
    h->indexUsed = il;
    h->index = calloc(h->indexAlloced, sizeof(*h->index));
    h->flags = HEADERFLAG_SORTED;
    h->nrefs = 0;
    h = headerLink(h);

    /*
     * XXX XFree86-libs, ash, and pdksh from Red Hat 5.2 have bogus
     * %verifyscript tag that needs to be diddled.
     */
    if (ntohl(pe->tag) == 15 &&
	ntohl(pe->type) == RPM_STRING_TYPE &&
	ntohl(pe->count) == 1)
    {
	pe->tag = htonl(1079);
    }

    entry = h->index;
    i = 0;
    if (!(htonl(pe->tag) < HEADER_I18NTABLE)) {
	h->flags |= HEADERFLAG_LEGACY;
	entry->info.type = REGION_TAG_TYPE;
	entry->info.tag = HEADER_IMAGE;
	/*@-sizeoftype@*/
	entry->info.count = REGION_TAG_COUNT;
	/*@=sizeoftype@*/
	entry->info.offset = ((char *)pe - dataStart); /* negative offset */

	/*@-assignexpose@*/
	entry->data = pe;
	/*@=assignexpose@*/
	entry->length = pvlen - sizeof(il) - sizeof(dl);
	rdlen = regionSwab(entry+1, il, 0, pe, dataStart, entry->info.offset);
#if 0	/* XXX don't check, the 8/98 i18n bug fails here. */
	if (rdlen != dl)
	    goto errxit;
#endif
	entry->rdlen = rdlen;
	entry++;
	h->indexUsed++;
    } else {
	int nb = ntohl(pe->count);
	int_32 rdl;
	int_32 ril;

	h->flags &= ~HEADERFLAG_LEGACY;

	entry->info.type = htonl(pe->type);
	if (entry->info.type < RPM_MIN_TYPE || entry->info.type > RPM_MAX_TYPE)
	    goto errxit;
	entry->info.count = htonl(pe->count);

	if (hdrchkTags(entry->info.count))
	    goto errxit;

	{   int off = ntohl(pe->offset);

	    if (hdrchkData(off))
		goto errxit;
	    if (off) {
		int_32 * stei = memcpy(alloca(nb), dataStart + off, nb);
		rdl = -ntohl(stei[2]);	/* negative offset */
		ril = rdl/sizeof(*pe);
		if (hdrchkTags(ril) || hdrchkData(rdl))
		    goto errxit;
		entry->info.tag = htonl(pe->tag);
	    } else {
		ril = il;
		/*@-sizeoftype@*/
		rdl = (ril * sizeof(struct entryInfo));
		/*@=sizeoftype@*/
		entry->info.tag = HEADER_IMAGE;
	    }
	}
	entry->info.offset = -rdl;	/* negative offset */

	/*@-assignexpose@*/
	entry->data = pe;
	/*@=assignexpose@*/
	entry->length = pvlen - sizeof(il) - sizeof(dl);
	rdlen = regionSwab(entry+1, ril-1, 0, pe+1, dataStart, entry->info.offset);
	if (rdlen < 0)
	    goto errxit;
	entry->rdlen = rdlen;

	if (ril < h->indexUsed) {
	    indexEntry newEntry = entry + ril;
	    int ne = (h->indexUsed - ril);
	    int rid = entry->info.offset+1;
	    int rc;

	    /* Load dribble entries from region. */
	    rc = regionSwab(newEntry, ne, 0, pe+ril, dataStart, rid);
	    if (rc < 0)
		goto errxit;
	    rdlen += rc;

	  { indexEntry firstEntry = newEntry;
	    int save = h->indexUsed;
	    int j;

	    /* Dribble entries replace duplicate region entries. */
	    h->indexUsed -= ne;
	    for (j = 0; j < ne; j++, newEntry++) {
		(void) headerRemoveEntry(h, newEntry->info.tag);
		if (newEntry->info.tag == HEADER_BASENAMES)
		    (void) headerRemoveEntry(h, HEADER_OLDFILENAMES);
	    }

	    /* If any duplicate entries were replaced, move new entries down. */
	    if (h->indexUsed < (save - ne)) {
		memmove(h->index + h->indexUsed, firstEntry,
			(ne * sizeof(*entry)));
	    }
	    h->indexUsed += ne;
	  }
	}
    }

    h->flags &= ~HEADERFLAG_SORTED;
    headerSort(h);

    /*@-globstate -observertrans @*/
    return h;
    /*@=globstate =observertrans @*/

errxit:
    /*@-usereleased@*/
    if (h) {
	h->index = _free(h->index);
	/*@-refcounttrans@*/
	h = _free(h);
	/*@=refcounttrans@*/
    }
    /*@=usereleased@*/
    /*@-refcounttrans -globstate@*/
    return h;
    /*@=refcounttrans =globstate@*/
}

/** \ingroup header
 * Read (and load) header from file handle.
 * @param fd		file handle
 * @param magicp	read (and verify) 8 bytes of (magic, 0)?
 * @return		header (or NULL on error)
 */
static /*@null@*/
Header headerRead(FD_t fd, enum hMagic magicp)
	/*@modifies fd @*/
{
    int_32 block[4];
    int_32 reserved;
    int_32 * ei = NULL;
    int_32 il;
    int_32 dl;
    int_32 magic;
    Header h = NULL;
    size_t len;
    int i;

    memset(block, 0, sizeof(block));
    i = 2;
    if (magicp == HEADER_MAGIC_YES)
	i += 2;

    /*@-type@*/ /* FIX: cast? */
    if (timedRead(fd, (char *)block, i*sizeof(*block)) != (i * sizeof(*block)))
	goto exit;
    /*@=type@*/

    i = 0;

    if (magicp == HEADER_MAGIC_YES) {
	magic = block[i++];
	if (memcmp(&magic, header_magic, sizeof(magic)))
	    goto exit;
	reserved = block[i++];
    }

    il = ntohl(block[i]);	i++;
    dl = ntohl(block[i]);	i++;

    /*@-sizeoftype@*/
    len = sizeof(il) + sizeof(dl) + (il * sizeof(struct entryInfo)) + dl;
    /*@=sizeoftype@*/

    /* Sanity checks on header intro. */
    if (hdrchkTags(il) || hdrchkData(dl) || len > headerMaxbytes)
	goto exit;

    ei = malloc(len);
    ei[0] = htonl(il);
    ei[1] = htonl(dl);
    len -= sizeof(il) + sizeof(dl);

    /*@-type@*/ /* FIX: cast? */
    if (timedRead(fd, (char *)&ei[2], len) != len)
	goto exit;
    /*@=type@*/

    h = headerLoad(ei);

exit:
    if (h) {
	if (h->flags & HEADERFLAG_ALLOCATED)
	    ei = _free(ei);
	h->flags |= HEADERFLAG_ALLOCATED;
    } else if (ei)
	ei = _free(ei);
    /*@-mustmod@*/	/* FIX: timedRead macro obscures annotation */
    return h;
    /*@-mustmod@*/
}

/**
 * Check package size.
 * @todo rpmio: use fdSize rather than fstat(2) to get file size.
 * @param fd			package file handle
 * @param siglen		signature header size
 * @param pad			signature padding
 * @param datalen		length of header+payload
 * @return 			rpmRC return code
 */
static rpmRC checkSize(FD_t fd, int siglen, int pad, int datalen)
	/*@globals fileSystem @*/
	/*@modifies fileSystem @*/
{
    rpmRC rc;

    rc = (((sizeof(struct rpmlead) + siglen + pad + datalen) - fd->len)
	? RPMRC_BADSIZE : RPMRC_OK);

    return rc;
}

/** \ingroup header
 * Create new (empty) header instance.
 * @return		header
 */
static
Header headerNew(void)
	/*@*/
{
    Header h = calloc(1, sizeof(*h));

    memset(h, 0, sizeof(*h));
    /*@-assignexpose@*/
    /*@=assignexpose@*/
    h->blob = NULL;
    h->indexAlloced = INDEX_MALLOC_SIZE;
    h->indexUsed = 0;
    h->flags = HEADERFLAG_SORTED;

    h->index = (h->indexAlloced
		? calloc(h->indexAlloced, sizeof(*h->index))
	: NULL);

    /*@-globstate -observertrans @*/
    h->nrefs = 0;
    return headerLink(h);
    /*@=globstate =observertrans @*/
}

typedef unsigned char byte;

static rpmRC rpmReadSignature(FD_t fd, Header * headerp, sigType sig_type)
{
    byte buf[2048];
    int sigSize, pad;
    int_32 type, count;
    int_32 *archSize;
    Header h = NULL;
    rpmRC rc = RPMRC_FAIL;		/* assume failure */

    if (headerp)
	*headerp = NULL;

    buf[0] = 0;
    switch (sig_type) {
    case RPMSIGTYPE_NONE:
	rc = RPMRC_OK;
	break;
    case RPMSIGTYPE_PGP262_1024:
	/* These are always 256 bytes */
	if (timedRead(fd, buf, 256) != 256)
	    break;
	h = headerNew();
	(void) headerAddEntry(h, RPMSIGTAG_PGP, RPM_BIN_TYPE, buf, 152);
	rc = RPMRC_OK;
	break;
    case RPMSIGTYPE_MD5:
    case RPMSIGTYPE_MD5_PGP:
	break;
    case RPMSIGTYPE_HEADERSIG:
    case RPMSIGTYPE_DISABLE:
	/* This is a new style signature */
	h = headerRead(fd, HEADER_MAGIC_YES);
	if (h == NULL)
	    break;

	rc = RPMRC_OK;
	sigSize = headerSizeof(h, HEADER_MAGIC_YES);

	/* XXX Legacy headers have a HEADER_IMAGE tag added. */
	if (headerIsEntry(h, RPMTAG_HEADERIMAGE))
	    sigSize -= (16 + 16);

	pad = (8 - (sigSize % 8)) % 8; /* 8-byte pad */
	if (sig_type == RPMSIGTYPE_HEADERSIG) {
	    if (! headerGetEntry(h, RPMSIGTAG_SIZE, &type,
				(void **)&archSize, &count))
		break;
	    rc = checkSize(fd, sigSize, pad, *archSize);
	}
	if (pad && timedRead(fd, buf, pad) != pad)
	    rc = RPMRC_SHORTREAD;
	break;
    default:
	break;
    }

    if (rc == 0 && headerp)
	/*@-nullderef@*/
	*headerp = h;
	/*@=nullderef@*/
    else if (h)
	h = headerFree(h);

    return rc;
}


static int readLead(FD_t fd, struct rpmlead *lead)
{
    memset(lead, 0, sizeof(*lead));
    /*@-type@*/ /* FIX: remove timed read */
    if (timedRead(fd, (char *)lead, sizeof(*lead)) != sizeof(*lead)) {
	return 1;
    }
    /*@=type@*/

    lead->type = ntohs(lead->type);
    lead->archnum = ntohs(lead->archnum);
    lead->osnum = ntohs(lead->osnum);

    if (lead->major >= 2)
	lead->signature_type = ntohs(lead->signature_type);

    return 0;
}

/** \ingroup header
 * Retrieve tag value using header internal array.
 * Get an entry using as little extra RAM as possible to return the tag value.
 * This is only an issue for RPM_STRING_ARRAY_TYPE.
 *
 * @param h		header
 * @param tag		tag
 * @retval type		address of tag value data type (or NULL)
 * @retval p		address of pointer to tag value(s) (or NULL)
 * @retval c		address of number of values (or NULL)
 * @return		1 on success, 0 on failure
 */
static
int headerGetEntryMinMemory(Header h, int_32 tag,
			/*@null@*/ /*@out@*/ hTYP_t type,
			/*@null@*/ /*@out@*/ hPTR_t * p,
			/*@null@*/ /*@out@*/ hCNT_t c)
	/*@modifies *type, *p, *c @*/
{
    return intGetEntry(h, tag, type, p, c, 1);
}

/** \ingroup header
 * Delete tag in header.
 * Removes all entries of type tag from the header, returns 1 if none were
 * found.
 *
 * @param h		header
 * @param tag		tag
 * @return		0 on success, 1 on failure (INCONSISTENT)
 */
static
int headerRemoveEntry(Header h, int_32 tag)
	/*@modifies h @*/
{
    indexEntry last = h->index + h->indexUsed;
    indexEntry entry, first;
    int ne;

    entry = findEntry(h, tag, RPM_NULL_TYPE);
    if (!entry) return 1;

    /* Make sure entry points to the first occurence of this tag. */
    while (entry > h->index && (entry - 1)->info.tag == tag)
	entry--;

    /* Free data for tags being removed. */
    for (first = entry; first < last; first++) {
	void * data;
	if (first->info.tag != tag)
	    break;
	data = first->data;
	first->data = NULL;
	first->length = 0;
	if (ENTRY_IN_REGION(first))
	    continue;
	data = _free(data);
    }

    ne = (first - entry);
    if (ne > 0) {
	h->indexUsed -= ne;
	ne = last - first;
	if (ne > 0)
	    memmove(entry, first, (ne * sizeof(*entry)));
    }

    return 0;
}

static int dncmp(const void * a, const void * b)
{
    const char *const * first = a;
    const char *const * second = b;
    return strcmp(*first, *second);
}

static void compressFilelist(Header h)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HAE_t hae = (HAE_t)headerAddEntry;
    HRE_t hre = (HRE_t)headerRemoveEntry;
    HFD_t hfd = headerFreeData;
    char ** fileNames;
    const char ** dirNames;
    const char ** baseNames;
    int_32 * dirIndexes;
    rpmTagType fnt;
    int count;
    int i;
    int dirIndex = -1;

    /*
     * This assumes the file list is already sorted, and begins with a
     * single '/'. That assumption isn't critical, but it makes things go
     * a bit faster.
     */

    if (headerIsEntry(h, RPMTAG_DIRNAMES)) {
	(void) hre(h, RPMTAG_OLDFILENAMES);
	return;		/* Already converted. */
    }

    if (!hge(h, RPMTAG_OLDFILENAMES, &fnt, (void **) &fileNames, &count))
	return;		/* no file list */
    if (fileNames == NULL || count <= 0)
	return;

    dirNames = alloca(sizeof(*dirNames) * count);	/* worst case */
    baseNames = alloca(sizeof(*dirNames) * count);
    dirIndexes = alloca(sizeof(*dirIndexes) * count);

    if (fileNames[0][0] != '/') {
	/* HACK. Source RPM, so just do things differently */
	dirIndex = 0;
	dirNames[dirIndex] = "";
	for (i = 0; i < count; i++) {
	    dirIndexes[i] = dirIndex;
	    baseNames[i] = fileNames[i];
	}
	goto exit;
    }

    for (i = 0; i < count; i++) {
	const char ** needle;
	char savechar;
	char * baseName;
	int len;

	if (fileNames[i] == NULL)	/* XXX can't happen */
	    continue;
	baseName = strrchr(fileNames[i], '/') + 1;
	len = baseName - fileNames[i];
	needle = dirNames;
	savechar = *baseName;
	*baseName = '\0';
	if (dirIndex < 0 ||
	    (needle = bsearch(&fileNames[i], dirNames, dirIndex + 1, sizeof(dirNames[0]), dncmp)) == NULL) {
	    char *s = alloca(len + 1);
	    memcpy(s, fileNames[i], len + 1);
	    s[len] = '\0';
	    dirIndexes[i] = ++dirIndex;
	    dirNames[dirIndex] = s;
	} else
	    dirIndexes[i] = needle - dirNames;

	*baseName = savechar;
	baseNames[i] = baseName;
    }

exit:
    if (count > 0) {
	(void) hae(h, RPMTAG_DIRINDEXES, RPM_INT32_TYPE, dirIndexes, count);
	(void) hae(h, RPMTAG_BASENAMES, RPM_STRING_ARRAY_TYPE,
			baseNames, count);
	(void) hae(h, RPMTAG_DIRNAMES, RPM_STRING_ARRAY_TYPE,
			dirNames, dirIndex + 1);
    }

    fileNames = hfd(fileNames, fnt);

    (void) hre(h, RPMTAG_OLDFILENAMES);
}

static int headerNVR(Header h, const char **np, const char **vp, const char **rp)
{
    int type;
    int count;

    if (np) {
	if (!(headerGetEntry(h, RPMTAG_NAME, &type, (void **) np, &count)
	    && type == RPM_STRING_TYPE && count == 1))
		*np = NULL;
    }
    if (vp) {
	if (!(headerGetEntry(h, RPMTAG_VERSION, &type, (void **) vp, &count)
	    && type == RPM_STRING_TYPE && count == 1))
		*vp = NULL;
    }
    if (rp) {
	if (!(headerGetEntry(h, RPMTAG_RELEASE, &type, (void **) rp, &count)
	    && type == RPM_STRING_TYPE && count == 1))
		*rp = NULL;
    }
    return 0;
}

/*
 * Up to rpm 3.0.4, packages implicitly provided their own name-version-release.
 * Retrofit an explicit "Provides: name = epoch:version-release.
 */
static void providePackageNVR(Header h)
{
    HGE_t hge = (HGE_t)headerGetEntryMinMemory;
    HFD_t hfd = headerFreeData;
    const char *name, *version, *release;
    int_32 * epoch;
    const char *pEVR;
    char *p;
    int_32 pFlags = RPMSENSE_EQUAL;
    const char ** provides = NULL;
    const char ** providesEVR = NULL;
    rpmTagType pnt, pvt;
    int_32 * provideFlags = NULL;
    int providesCount;
    int i;
    int bingo = 1;

    /* Generate provides for this package name-version-release. */
    (void) headerNVR(h, &name, &version, &release);
    if (!(name && version && release))
	return;
    pEVR = p = alloca(21 + strlen(version) + 1 + strlen(release) + 1);
    *p = '\0';
    if (hge(h, RPMTAG_EPOCH, NULL, (void **) &epoch, NULL)) {
	sprintf(p, "%d:", *epoch);
	while (*p != '\0')
	    p++;
    }
    (void) my_stpcpy( my_stpcpy( my_stpcpy(p, version) , "-") , release);

    /*
     * Rpm prior to 3.0.3 does not have versioned provides.
     * If no provides at all are available, we can just add.
     */
    if (!hge(h, RPMTAG_PROVIDENAME, &pnt, (void **) &provides, &providesCount))
	goto exit;

    /*
     * Otherwise, fill in entries on legacy packages.
     */
    if (!hge(h, RPMTAG_PROVIDEVERSION, &pvt, (void **) &providesEVR, NULL)) {
	for (i = 0; i < providesCount; i++) {
	    char * vdummy = "";
	    int_32 fdummy = RPMSENSE_ANY;
	    (void) headerAddOrAppendEntry(h, RPMTAG_PROVIDEVERSION, RPM_STRING_ARRAY_TYPE,
			&vdummy, 1);
	    (void) headerAddOrAppendEntry(h, RPMTAG_PROVIDEFLAGS, RPM_INT32_TYPE,
			&fdummy, 1);
	}
	goto exit;
    }

    (void) hge(h, RPMTAG_PROVIDEFLAGS, NULL, (void **) &provideFlags, NULL);

    if (provides && providesEVR && provideFlags)
    for (i = 0; i < providesCount; i++) {
        if (!(provides[i] && providesEVR[i]))
            continue;
	if (!(provideFlags[i] == RPMSENSE_EQUAL &&
	    !strcmp(name, provides[i]) && !strcmp(pEVR, providesEVR[i])))
	    continue;
	bingo = 0;
	break;
    }

exit:
    provides = hfd(provides, pnt);
    providesEVR = hfd(providesEVR, pvt);

    if (bingo) {
	(void) headerAddOrAppendEntry(h, RPMTAG_PROVIDENAME, RPM_STRING_ARRAY_TYPE,
		&name, 1);
	(void) headerAddOrAppendEntry(h, RPMTAG_PROVIDEFLAGS, RPM_INT32_TYPE,
		&pFlags, 1);
	(void) headerAddOrAppendEntry(h, RPMTAG_PROVIDEVERSION, RPM_STRING_ARRAY_TYPE,
		&pEVR, 1);
    }
}

static Header rpmFreeSignature(Header h);

/**
 * Retrieve package components from file handle.
 * @param fd		file handle
 * @param leadPtr	address of lead (or NULL)
 * @param sigs		address of signatures (or NULL)
 * @param hdrPtr	address of header (or NULL)
 * @return		rpmRC return code
 */
static rpmRC readPackageHeaders(FD_t fd,
		/*@null@*/ /*@out@*/ struct rpmlead * leadPtr,
		/*@null@*/ /*@out@*/ Header * sigs,
		/*@null@*/ /*@out@*/ Header * hdrPtr)
	/*@modifies fd, *leadPtr, *sigs, *hdrPtr @*/
{
    Header hdrBlock;
    struct rpmlead leadBlock;
    Header * hdr = NULL;
    struct rpmlead * lead;
    char * defaultPrefix;
    rpmRC rc;

    hdr = hdrPtr ? hdrPtr : &hdrBlock;
    lead = leadPtr ? leadPtr : &leadBlock;

    if (readLead(fd, lead))
	return RPMRC_FAIL;

    if (lead->magic[0] != RPMLEAD_MAGIC0 || lead->magic[1] != RPMLEAD_MAGIC1 ||
	lead->magic[2] != RPMLEAD_MAGIC2 || lead->magic[3] != RPMLEAD_MAGIC3) {
	return RPMRC_BADMAGIC;
    }

    switch (lead->major) {
    case 1:
	return RPMRC_FAIL;
	/*@notreached@*/ break;
    case 2:
    case 3:
    case 4:
	rc = rpmReadSignature(fd, sigs, lead->signature_type);
	if (rc == RPMRC_FAIL)
	    return rc;
	*hdr = headerRead(fd, (lead->major >= 3)
			  ? HEADER_MAGIC_YES : HEADER_MAGIC_NO);
	if (*hdr == NULL) {
	    if (sigs != NULL)
		*sigs = rpmFreeSignature(*sigs);
	    return RPMRC_FAIL;
	}

	/*
	 * We don't use these entries (and rpm >= 2 never has) and they are
	 * pretty misleading. Let's just get rid of them so they don't confuse
	 * anyone.
	 */
	if (headerIsEntry(*hdr, RPMTAG_FILEUSERNAME))
	    (void) headerRemoveEntry(*hdr, RPMTAG_FILEUIDS);
	if (headerIsEntry(*hdr, RPMTAG_FILEGROUPNAME))
	    (void) headerRemoveEntry(*hdr, RPMTAG_FILEGIDS);

	/*
	 * We switched the way we do relocateable packages. We fix some of
	 * it up here, though the install code still has to be a bit
	 * careful. This fixup makes queries give the new values though,
	 * which is quite handy.
	 */
	if (headerGetEntry(*hdr, RPMTAG_DEFAULTPREFIX, NULL,
			   (void **) &defaultPrefix, NULL))
	{
	    defaultPrefix =
	      stripTrailingChar(alloca_strdup(defaultPrefix), '/');
	    (void) headerAddEntry(*hdr, RPMTAG_PREFIXES, RPM_STRING_ARRAY_TYPE,
			   &defaultPrefix, 1);
	}

	/*
	 * The file list was moved to a more compressed format which not
	 * only saves memory (nice), but gives fingerprinting a nice, fat
	 * speed boost (very nice). Go ahead and convert old headers to
	 * the new style (this is a noop for new headers).
	 */
	if (lead->major < 4)
	    compressFilelist(*hdr);

    /* XXX binary rpms always have RPMTAG_SOURCERPM, source rpms do not */
        if (lead->type == RPMLEAD_SOURCE) {
	    int_32 one = 1;
	    if (!headerIsEntry(*hdr, RPMTAG_SOURCEPACKAGE))
	      (void)headerAddEntry(*hdr, RPMTAG_SOURCEPACKAGE, RPM_INT32_TYPE,
				&one, 1);
	} else if (lead->major < 4) {
	    /* Retrofit "Provide: name = EVR" for binary packages. */
	    providePackageNVR(*hdr);
	}
	break;

    default:
	return RPMRC_FAIL;
	/*@notreached@*/ break;
    }

    if (hdrPtr == NULL)
	*hdr = headerFree(*hdr);

    return RPMRC_OK;
}

static void headerMergeLegacySigs(Header h, const Header sig)
{
    HFD_t hfd = (HFD_t) headerFreeData;
    HAE_t hae = (HAE_t) headerAddEntry;
    HeaderIterator hi;
    int_32 tag, type, count;
    const void * ptr;
    int xx;

    /*@-mods@*/ /* FIX: undocumented modification of sig */
    for (hi = headerInitIterator(sig);
    /*@=mods@*/
        headerNextIterator(hi, &tag, &type, &ptr, &count, 0);
        ptr = hfd(ptr, type))
    {
	switch (tag) {
	case RPMSIGTAG_SIZE:
	    tag = RPMTAG_SIGSIZE;
	    /*@switchbreak@*/ break;
	case RPMSIGTAG_LEMD5_1:
	    tag = RPMTAG_SIGLEMD5_1;
	    /*@switchbreak@*/ break;
	case RPMSIGTAG_PGP:
	    tag = RPMTAG_SIGPGP;
	    /*@switchbreak@*/ break;
	case RPMSIGTAG_LEMD5_2:
	    tag = RPMTAG_SIGLEMD5_2;
	    /*@switchbreak@*/ break;
	case RPMSIGTAG_MD5:
	    tag = RPMTAG_SIGMD5;
	    /*@switchbreak@*/ break;
	case RPMSIGTAG_GPG:
	    tag = RPMTAG_SIGGPG;
	    /*@switchbreak@*/ break;
	case RPMSIGTAG_PGP5:
	    tag = RPMTAG_SIGPGP5;
	    /*@switchbreak@*/ break;
	case RPMSIGTAG_PAYLOADSIZE:
	    tag = RPMTAG_ARCHIVESIZE;
	    /*@switchbreak@*/ break;
	case RPMSIGTAG_SHA1:
	case RPMSIGTAG_DSA:
	case RPMSIGTAG_RSA:
	default:
	    if (!(tag >= HEADER_SIGBASE && tag < HEADER_TAGBASE))
		continue;
	    /*@switchbreak@*/ break;
	}
	if (ptr == NULL) continue;	/* XXX can't happen */
	if (!headerIsEntry(h, tag))
	    xx = hae(h, tag, type, ptr, count);
    }
    hi = headerFreeIterator(hi);
}

static Header rpmFreeSignature(Header h)
{
    return headerFree(h);
}

static rpmRC rpmReadPackageHeader(FD_t fd, Header * hdrp, int * isSource, int * major,
				  int * minor)
{
    struct rpmlead lead;
    Header sig = NULL;
    rpmRC rc = readPackageHeaders(fd, &lead, &sig, hdrp);

    if (rc != RPMRC_OK)
	goto exit;

    if (hdrp && *hdrp && sig) {
	headerMergeLegacySigs(*hdrp, sig);
	sig = rpmFreeSignature(sig);
    }

    if (isSource) *isSource = lead.type == RPMLEAD_SOURCE;
    /*@-mods@*/
    if (major) *major = lead.major;
    if (minor) *minor = lead.minor;
    /*@=mods@*/

exit:
    return rc;
}


/* ******************** real libextractor stuff ************************ */

static struct EXTRACTOR_Keywords * addKeyword(EXTRACTOR_KeywordType type,
					      char * keyword,
					      struct EXTRACTOR_Keywords * next) {
  EXTRACTOR_KeywordList * result;

  if (keyword == NULL)
    return next;
  result = malloc(sizeof(EXTRACTOR_KeywordList));
  result->next = next;
  result->keyword = strdup(keyword);
  result->keywordType = type;
  return result;
}

typedef struct {
  int_32 rtype;
  EXTRACTOR_KeywordType type;
} Matches;

/* SEE:
   http://rikers.org/rpmbook/node119.html#SECTION031425200000000000000 */
static Matches tests[] = {
  { RPMTAG_NAME, EXTRACTOR_TITLE },
  { RPMTAG_VERSION, EXTRACTOR_VERSIONNUMBER },
  { RPMTAG_RELEASE, EXTRACTOR_RELEASE },
  { RPMTAG_GROUP, EXTRACTOR_GROUP },
  { RPMTAG_SIZE, EXTRACTOR_SIZE },
  { RPMTAG_URL, EXTRACTOR_RESOURCE_IDENTIFIER },
  { RPMTAG_SUMMARY, EXTRACTOR_SUMMARY },
  { RPMTAG_PACKAGER, EXTRACTOR_PACKAGER },
  { RPMTAG_BUILDTIME, EXTRACTOR_CREATION_DATE },
  { RPMTAG_COPYRIGHT, EXTRACTOR_COPYRIGHT },
  { RPMTAG_LICENSE, EXTRACTOR_LICENSE },
  { RPMTAG_DISTRIBUTION, EXTRACTOR_DISTRIBUTION },
  { RPMTAG_BUILDHOST, EXTRACTOR_BUILDHOST },
  { RPMTAG_VENDOR, EXTRACTOR_VENDOR },
  { RPMTAG_OS, EXTRACTOR_OS },
  { RPMTAG_DESCRIPTION, EXTRACTOR_DESCRIPTION },
  { 0, 0},
};

/* mimetype = application/rpm */
struct EXTRACTOR_Keywords * libextractor_rpm_extract(char * filename,
                                                     char * data,
                                                     size_t size,
                                                     struct EXTRACTOR_Keywords * prev) {
  fdStruct handle;
  Header hdr;
  HeaderIterator hi;
  int_32 tag;
  int_32 type;
  int_32 c;
  hPTR_t p;
  int isSource;
  int major;
  int minor;
  int i;
  char verb[40];

  handle.data = data;
  handle.pos = 0;
  handle.len = size;
  if (0 != rpmReadPackageHeader(&handle,
				&hdr,
				&isSource,
				&major,
				&minor)) {
    return prev;
  }
  prev = addKeyword(EXTRACTOR_MIMETYPE,
		    "application/x-redhat-package-manager",
		    prev);
  /* FIXME: add platform! */
  if (isSource)
    sprintf(verb,
	    _("Source RPM %d.%d"),
	    major,
	    minor);
  else
    sprintf(verb,
	    _("Binary RPM %d.%d"),
	    major,
	    minor);
  prev = addKeyword(EXTRACTOR_UNKNOWN,
		    verb,
		    prev);
  hi = headerInitIterator(hdr);
  while (1 == headerNextIterator(hi,
				 &tag,
				 &type,
				 &p,
				 &c,
				 0)) {
    i=0;
    while (tests[i].rtype != 0) {
      if (tests[i].rtype == tag) {
	switch (type) {
	case RPM_STRING_ARRAY_TYPE: {	
	  char * tmp;
	  const char * p2;
	  int c2;
	  int size;

	  c2 = c;
	  p2 = p;
	  size = 0;
	  while (c2--) {
	    size += strlen(p2);
	    p2 = strchr(p2, 0);
	    p2++;
	  }
	
	  tmp = malloc(size+1);
	  tmp[0] = '\0';
	  while (c--) {
	    strcat(tmp,  p);
	    p = strchr(p, 0);
	    p++;
	  }
	  prev = addKeyword(tests[i].type,
			    tmp,
			    prev);	
	  free(tmp);
	  break;
	}
	case RPM_I18NSTRING_TYPE: {	
	  char * tmp;
	  const char * p2;
	  int c2;
	  int size;

	  c2 = c;
	  p2 = p;
          p2+=sizeof(char*)*c;
	  size = 0;
	  while (c2--) {
	    size += strlen(p2);
	    p2 = strchr(p2, 0);
	    p2++;
	  }
	
	  tmp = malloc(size+1);
	  tmp[0] = '\0';
	  p2 = p;
          p2+=sizeof(char*)*c;
	  while (c--) {
	    strcat(tmp, p2);
	    p2 = strchr(p2, 0);
	    p2++;
	  }
	  prev = addKeyword(tests[i].type,
			    tmp,
			    prev);	
	  free(tmp);
	  break;
	}
	case RPM_STRING_TYPE:
	  prev = addKeyword(tests[i].type,
			    (char*)p,
			    prev);
	  break;
	case RPM_INT32_TYPE: {
	  if (tag == RPMTAG_BUILDTIME) {
	    char tmp[30];

	    ctime_r((time_t*)p, tmp);
	    tmp[strlen(tmp)-1] = '\0'; /* eat linefeed */
	    prev = addKeyword(tests[i].type,
			      tmp,
			      prev);
	  } else {
	    char tmp[14];
	
	    sprintf(tmp, "%d", *(int*)p);
	    prev = addKeyword(tests[i].type,
			      tmp,
			      prev);
	  }
	  break;
	}
	}
      }	
      i++;
    }
    if ( ( (type == RPM_BIN_TYPE) ||
	   (type == RPM_I18NSTRING_TYPE) ||
	   (type == RPM_STRING_ARRAY_TYPE) ) &&
 	 (p != NULL) ) {
      free((void*)p);
    }
  }
  headerFreeIterator(hi);
  headerFree(hdr);
  return prev;
}




/* end of rpmextractor.c */
