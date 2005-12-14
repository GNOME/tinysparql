/*
     This file is part of libextractor.
     (C) 2002, 2003, 2004, 2005 Vidyut Samanta and Christian Grothoff

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
#include "getopt.h"

#define YES 1
#define NO 0


typedef struct {
  char shortArg;
  char * longArg;
  char * mandatoryArg;
  char * description;
} Help;

#define BORDER 29

static void formatHelp(const char * general,
		       const char * description,
		       const Help * opt) {
  int slen;
  int i;
  int j;
  int ml;
  int p;
  char * scp;
  const char * trans;
	
  printf(_("Usage: %s\n%s\n\n"),
	 gettext(general),
	 gettext(description));
  printf(_("Arguments mandatory for long options are also mandatory for short options.\n"));
  slen = 0;
  i = 0;
  while (opt[i].description != NULL) {
    if (opt[i].shortArg == 0)
      printf("      ");
    else
      printf("  -%c, ",
	     opt[i].shortArg);
    printf("--%s",
	   opt[i].longArg);
    slen = 8 + strlen(opt[i].longArg);
    if (opt[i].mandatoryArg != NULL) {
      printf("=%s",
	     opt[i].mandatoryArg);
      slen += 1+strlen(opt[i].mandatoryArg);
    }
    if (slen > BORDER) {
      printf("\n%*s", BORDER, "");
      slen = BORDER;
    }
    if (slen < BORDER) {
      printf("%*s", BORDER-slen, "");
      slen = BORDER;
    }
    trans = gettext(opt[i].description);
    ml = strlen(trans);
    p = 0;
  OUTER:
    while (ml - p > 78 - slen) {
      for (j=p+78-slen;j>p;j--) {
	if (isspace(trans[j])) {
	  scp = malloc(j-p+1);
	  memcpy(scp,
		 &trans[p],
		 j-p);
	  scp[j-p] = '\0';
	  printf("%s\n%*s",
		 scp,
		 BORDER+2,
		 "");
	  free(scp);
	  p = j+1;
	  slen = BORDER+2;
	  goto OUTER;
	}
      }
      /* could not find space to break line */
      scp = malloc(78 - slen + 1);
      memcpy(scp,
	     &trans[p],
	     78 - slen);
      scp[78 - slen] = '\0';
      printf("%s\n%*s",
	     scp,
	     BORDER+2,
	     "");	
      free(scp);
      slen = BORDER+2;
      p = p + 78 - slen;
    }
    /* print rest */
    if (p < ml)
      printf("%s\n",
	     &trans[p]);
    i++;
  }
}

static void
printHelp ()
{
  static Help help[] = {
    { 'a', "all", NULL,
      gettext_noop("do not remove any duplicates") },
    { 'b', "bibtex", NULL,
      gettext_noop("print output in bibtex format") },
    { 'B', "binary", "LANG",
      gettext_noop("use the generic plaintext extractor for the language with the 2-letter language code LANG") },
    { 'd', "duplicates", NULL,
      gettext_noop("remove duplicates only if types match") },
    { 'f', "filename", NULL,
      gettext_noop("use the filename as a keyword (loads filename-extractor plugin)") },
    { 'h', "help", NULL,
      gettext_noop("print this help") },
    { 'H', "hash", "ALGORITHM",
      gettext_noop("compute hash using the given ALGORITHM (currently sha1 or md5)") },
    { 'l', "library", "LIBRARY",
      gettext_noop("load an extractor plugin named LIBRARY") },
    { 'L', "list", NULL,
      gettext_noop("list all keyword types") },
    { 'n', "nodefault", NULL,
      gettext_noop("do not use the default set of extractor plugins") },
    { 'p', "print", "TYPE",
      gettext_noop("print only keywords of the given TYPE (use -L to get a list)") },
    { 'r', "remove-duplicates", NULL,
      gettext_noop("remove duplicates even if keyword types do not match") },
    { 's', "split", NULL,
      gettext_noop("use keyword splitting (loads split-extractor plugin)") },
    { 'v', "version", NULL,
      gettext_noop("print the version number") },
    { 'V', "verbose", NULL,
      gettext_noop("be verbose") },
    { 'x', "exclude", "TYPE",
      gettext_noop("do not print keywords of the given TYPE") },
    { 0, NULL, NULL, NULL },
  };
  formatHelp(_("extract [OPTIONS] [FILENAME]*"),
	     _("Extract metadata from files."),
	     help);

}

#include "iconv.c"


/**
 * Print a keyword list to a file.
 * For debugging.
 * @param handle the file to write to (stdout, stderr), may NOT be NULL
 * @param keywords the list of keywords to print, may be NULL
 * @param print array indicating which types to print
 */
static void
printSelectedKeywords(FILE * handle,
		      EXTRACTOR_KeywordList * keywords,
		      const int * print,
		      const int verbose)
{
  char * keyword;
  iconv_t cd;
  char * buf;

  cd = iconv_open(
    nl_langinfo(CODESET)
    , "UTF-8");
  while (keywords != NULL) {
    buf = NULL;
    if (cd != (iconv_t) -1)
      keyword = iconvHelper(cd,
			    keywords->keyword);
    else
      keyword = strdup(keywords->keyword);

    if (keywords->keywordType == EXTRACTOR_THUMBNAIL_DATA) {
      fprintf (handle,
	       _("%s - (binary)\n"),
	       EXTRACTOR_getKeywordTypeAsString(keywords->keywordType));
    } else {
      if (NULL == EXTRACTOR_getKeywordTypeAsString(keywords->keywordType)) {
	if (verbose == YES) {
	  fprintf(handle,
		  _("INVALID TYPE - %s\n"),
		  keyword);
	}
      } else if (print[keywords->keywordType] == YES)
	fprintf (handle,
		 "%s - %s\n",
		 EXTRACTOR_getKeywordTypeAsString(keywords->keywordType),
		 keyword);
    }
    free(keyword);
    keywords = keywords->next;
  }
  if (cd != (iconv_t) -1)
    iconv_close(cd);
}

/**
 * Take title, auth, year and return a string
 */
static char *
splice(const char * title,
       const char * auth,
       const char * year)
{
  char * temp = (char*)malloc(sizeof(char)*16);
  int i = 0;
  memset(temp, 0, sizeof(char)*16);
  snprintf(temp, 15, "%.5s%.5s%.5s", auth, year, title);
  for ( i = 0; i < strlen(temp); i++ ) {
    if ( !isalnum(temp[i]) )
      temp[i] = '_';
    else
      temp[i] = tolower(temp[i]);
  }
  return temp;
}

/**
 * Print a keyword list in bibtex format to a file.
 * FIXME: We should generate the three letter abbrev of the month
 * @param handle the file to write to (stdout, stderr), may NOT be NULL
 * @param keywords the list of keywords to print, may be NULL
 * @param print array indicating which types to print
 */
static void
printSelectedKeywordsBibtex (FILE * handle,
			     EXTRACTOR_KeywordList * keywords,
			     const int * print,
			     const char * filename)
{
  const char * last = NULL;
  if (keywords == NULL)
    return;
  if (print[keywords->keywordType] == YES)
    {
      const char * title = NULL;
      const char * author = NULL;
      const char * note = NULL;
      const char * date = NULL;
      const char * publisher = NULL;
      const char * organization = NULL;
      const char * key = NULL;
      const char * pages = NULL;
      char * year = NULL;
      char * month = NULL;

      title = EXTRACTOR_extractLastByString(_("title"), keywords);
      if ( !title )
	title = EXTRACTOR_extractLastByString(_("filename"), keywords);
      if ( !title )
	title = (char*)filename;
      last = title;

      author = EXTRACTOR_extractLastByString(_("author"), keywords);
      if ( author )
	last = author;

      note = EXTRACTOR_extractLastByString(_("description"), keywords);
      if ( !note )
	note = EXTRACTOR_extractLastByString(_("keywords"), keywords);
      if ( !note )
	note = EXTRACTOR_extractLastByString(_("comment"), keywords);
      if ( note )
	last = note;

      date = EXTRACTOR_extractLastByString(_("date"), keywords);
      if ( !date )
	date = EXTRACTOR_extractLastByString(_("creation date"), keywords);
      if ( date ) {
	if ( strlen(keywords->keyword) >= 7 ) {
	  year = (char*)malloc(sizeof(char)*5);
	  memset(year, 0, sizeof(char)*5);
	  month = (char*)malloc(sizeof(char)*3);
	  memset(month, 0, sizeof(char)*3);
	  year[0] = keywords->keyword[0];
	  year[1] = keywords->keyword[1];
	  year[2] = keywords->keyword[2];
	  year[3] = keywords->keyword[3];
	  month[0] = keywords->keyword[4];
	  month[1] = keywords->keyword[5];
	} else if ( strlen(keywords->keyword) >= 4 ) {
	  year = (char*)malloc(sizeof(char)*5);
	  memset(year, 0, sizeof(char)*5);
	  year[0] = keywords->keyword[0];
	  year[1] = keywords->keyword[1];
	  year[2] = keywords->keyword[2];
	  year[3] = keywords->keyword[3];
	}
      }
      if ( year )
	last = year;

      if ( month )
	last = month;

      publisher = EXTRACTOR_extractLastByString(_("publisher"), keywords);
      if ( publisher )
	last = publisher;

      organization = EXTRACTOR_extractLastByString(_("organization"), keywords);
      if ( organization )
	last = organization;

      key = EXTRACTOR_extractLastByString(_("subject"), keywords);
      if ( key )
	last = key;

      pages = EXTRACTOR_extractLastByString(_("page count"), keywords);
      if ( pages )
	last = pages;

      fprintf(handle, "@misc{ %s,\n",splice(title, author, year));
      if ( title )
	fprintf(handle, "    title = \"%s\"%s\n", title,
	    (last == title)?"":",");
      if ( author )
	fprintf(handle, "    author = \"%s\"%s\n", author,
	    (last == author)?"":",");
      if ( note )
	fprintf(handle, "    note = \"%s\"%s\n", note,
	    (last == note)?"":",");
      if ( year )
	fprintf(handle, "    year = \"%s\"%s\n", year,
	    (last == year)?"":",");
      if ( month )
	fprintf(handle, "    month = \"%s\"%s\n", month,
	    (last == month)?"":",");
      if ( publisher )
	fprintf(handle, "    publisher = \"%s\"%s\n", publisher,
	    (last == publisher)?"":",");
      if ( organization )
	fprintf(handle, "    organization = \"%s\"%s\n", organization,
	    (last == organization)?"":",");
      if ( key )
	fprintf(handle, "    key = \"%s\"%s\n", key,
	    (last == key)?"":",");
      if ( pages )
	fprintf(handle, "    pages = \"%s\"%s\n", pages,
	    (last == pages)?"":",");

      fprintf(handle, "}\n\n");
    }
}

/**
 * Demo for libExtractor.
 * <p>
 * Invoke with a list of filenames to extract keywords
 * from (demo will use all the extractor libraries that
 * are available by default).
 */
int
main (int argc, char *argv[])
{
  int i;
  EXTRACTOR_ExtractorList *extractors;
  EXTRACTOR_KeywordList *keywords;
  int option_index;
  int c;
  char * libraries = NULL;
  char * hash = NULL;
  int splitKeywords = NO;
  int verbose = NO;
  int useFilename = NO;
  int nodefault = NO;
  int *print;
  int defaultAll = YES;
  int duplicates = EXTRACTOR_DUPLICATES_REMOVE_UNKNOWN;
  int bibtex = NO;
  char * binary = NULL;
  int ret = 0;

#ifdef MINGW
  InitWinEnv();
#endif
#if ENABLE_NLS
  setlocale(LC_ALL, "");
  textdomain("libextractor");
  BINDTEXTDOMAIN("libextractor", LOCALEDIR);
#endif
  print = malloc (sizeof (int) * EXTRACTOR_getHighestKeywordTypeNumber ());
  for (i = 0; i < EXTRACTOR_getHighestKeywordTypeNumber (); i++)
    print[i] = YES;		/* default: print everything */

  while (1)
    {
      static struct option long_options[] = {
	{"all", 0, 0, 'a'},
	{"binary", 1, 0, 'B'},
	{"bibtex", 0, 0, 'b'},
	{"duplicates", 0, 0, 'd'},
	{"filename", 0, 0, 'f'},
	{"help", 0, 0, 'h'},
	{"hash", 1, 0, 'H'},
	{"list", 0, 0, 'L'},
	{"library", 1, 0, 'l'},
	{"nodefault", 0, 0, 'n'},
	{"print", 1, 0, 'p'},
	{"remove-duplicates", 0, 0, 'r'},
	{"split", 0, 0, 's'},
	{"verbose", 0, 0, 'V'},
	{"version", 0, 0, 'v'},
	{"exclude", 1, 0, 'x'},
	{0, 0, 0, 0}
      };
      option_index = 0;
      c = getopt_long (argc,
		       argv, "vhbl:nsH:fp:x:LVdraB:",
		       long_options,
		       &option_index);

      if (c == -1)
	break;			/* No more flags to process */
      switch (c)
	{
	case 'a':
	  duplicates = -1;
	  break;
	case 'b':
	  bibtex = YES;
	  break;
	case 'B':
	  binary = optarg;
	  break;
	case 'd':
	  duplicates = 0;
	  break;
	case 'f':
	  useFilename = YES;
	  break;
	case 'h':
	  printHelp();
	  return 0;
	case 'H':
	  hash = optarg;
	  break;
	case 'l':
	  libraries = optarg;
	  break;
	case 'L':
	  i = 0;
	  while (NULL != EXTRACTOR_getKeywordTypeAsString (i))
	    printf ("%s\n", EXTRACTOR_getKeywordTypeAsString (i++));
	  return 0;
	case 'n':
	  nodefault = YES;
	  break;
	case 'p':
	  if (optarg == NULL) {
	    fprintf(stderr,
		    _("You must specify an argument for the `%s' option (option ignored).\n"),
		    "-p");
	    break;
	  }
	  if (defaultAll == YES)
	    {
	      defaultAll = NO;
	      i = 0;
	      while (NULL != EXTRACTOR_getKeywordTypeAsString (i))
		print[i++] = NO;
	    }
	  i = 0;
	  while (NULL != EXTRACTOR_getKeywordTypeAsString (i))
	    {
	      if (0 == strcmp (optarg, EXTRACTOR_getKeywordTypeAsString (i)))
		{
		  print[i] = YES;
		  break;
		}
	      i++;
	    }
	  if (NULL == EXTRACTOR_getKeywordTypeAsString (i))
	    {
	      fprintf(stderr,
		      "Unknown keyword type `%s', use option `%s' to get a list.\n",
		      optarg,
		       "-L");
	      return -1;
	    }
	  break;
	case 'r':
	  duplicates = EXTRACTOR_DUPLICATES_TYPELESS;
	  break;
	case 's':
	  splitKeywords = YES;
	  break;
       	case 'v':
	  printf ("extract v%s\n", PACKAGE_VERSION);
	  return 0;
	case 'V':
	  verbose = YES;
	  break;
	case 'x':
	  i = 0;
	  while (NULL != EXTRACTOR_getKeywordTypeAsString (i))
	    {
	      if (0 == strcmp (optarg, EXTRACTOR_getKeywordTypeAsString (i)))
		{
		  print[i] = NO;
		  break;
		}
	      i++;
	    }
	  if (NULL == EXTRACTOR_getKeywordTypeAsString (i))
	    {
	      fprintf (stderr,
		       "Unknown keyword type `%s', use option `%s' to get a list.\n",
		       optarg,
		       "-L");
#ifdef MINGW
  			ShutdownWinEnv();
#endif
	      return -1;
	    }
	  break;
	default:
	  fprintf (stderr,
		   _("Use --help to get a list of options.\n"));
#ifdef MINGW
  	ShutdownWinEnv();
#endif
	  return -1;
	}			/* end of parsing commandline */
    }				/* while (1) */

  if (argc - optind < 1)
    {
      fprintf (stderr,
	       "Invoke with list of filenames to extract keywords form!\n");
#ifdef MINGW
  		ShutdownWinEnv();
#endif
      return -1;
    }

  /* build list of libraries */
  if (nodefault == NO)
    extractors = EXTRACTOR_loadDefaultLibraries ();
  else
    extractors = NULL;
  if (useFilename == YES)
    extractors = EXTRACTOR_addLibrary (extractors,
				       "libextractor_filename");
  if (libraries != NULL)
    extractors = EXTRACTOR_loadConfigLibraries (extractors, libraries);

  if (binary != NULL) {
    char * name;
    name = malloc(strlen(binary) + strlen("libextractor_printable_") + 1);
    strcpy(name, "libextractor_printable_");
    strcat(name, binary);
    extractors = EXTRACTOR_addLibraryLast(extractors,
					  name);
    free(name);
  }
  if (hash != NULL) {
    char * name;
    name = malloc(strlen(hash) + strlen("libextractor_hash_") + 1);
    strcpy(name, "libextractor_hash_");
    strcat(name, hash);
    extractors = EXTRACTOR_addLibraryLast(extractors,
					  name);
    free(name);
  }

  if (splitKeywords == YES)
    extractors = EXTRACTOR_addLibraryLast(extractors,
					  "libextractor_split");

  if (verbose == YES)
    {
      /* print list of all used extractors */
    }
  /* extract keywords */
  if ( bibtex == YES )
    fprintf(stdout,
	    _("%% BiBTeX file\n"));
  for (i = optind; i < argc; i++)
    {
      errno = 0;
      keywords = EXTRACTOR_getKeywords (extractors, argv[i]);
      if (0 != errno) {
	if (verbose == YES) {
	  fprintf(stderr, 
		  "%s: %s: %s\n",
		  argv[0], argv[i], strerror(errno));
	}
	ret = 1;
	continue;
      }
      if (duplicates != -1 || bibtex == YES)
	keywords = EXTRACTOR_removeDuplicateKeywords (keywords, duplicates);
      if (verbose == YES && bibtex == NO)
	printf (_("Keywords for file %s:\n"), argv[i]);
      if (bibtex == YES)
	printSelectedKeywordsBibtex (stdout, keywords, print, argv[i]);
      else
	printSelectedKeywords (stdout, keywords, print, verbose);
      if (verbose == YES && bibtex == NO)
	printf ("\n");
      EXTRACTOR_freeKeywords (keywords);
    }
  free (print);
  EXTRACTOR_removeAll (extractors);

#ifdef MINGW
  ShutdownWinEnv();
#endif

  return ret;
}
