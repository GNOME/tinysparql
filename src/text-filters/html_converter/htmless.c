/*	HTMLess, a program to read an HTML document and output two files: a formatted text file
 * 	and a semi-formatted file containing the HTML tags.  
 *
 *	HTMLess Version 1.0 (Created 12/95)
 *	Copyright (C) 1995-1999. Stephen M. Orth
 *	Email: sorth@oz.net
 *
 *	This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published
 *	by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *	My main objectives in writing this program:
 *	1) Make it easy to spell check a HTML document without the spell-checker
 *	stopping at every tag
 *	2) Allow for easy reading of HTML files without starting a Web browser
 *	3) Format the textfile in a way similar in appearance to how it would appear
 *	when opened with a browser.
 *	4) Allow for basic debugging of HTML source by showing all tags in a semi-structured
 *	order--you can find missing tags quickly.
 *
 *	Program makes the assumption that the text within an HTML tag is either all upper case, or all
 *	lower case.
 *
 *	Revisions:
 *
 *	March 2, 1996 (version 1.1)
 *
 *	Added ability to enter filenames for output.
 *	Began making program more modular
 *
 *	May 3, 1996   (version 1.11)
 *
 *	added check for response to files exists other than y or n
 *	fixed character overflow problem occuring with long tags
 *	(i.e. JavaScript) by increasing MAXLENGTH
 *
 *	September 3, 1996  (version 1.2)
 *
 *	Utilized argc and argv to allow program to accept command line
 *	args.
 *
 *	Feburary, 1998 (version 1.21) added support for HTML special characters
 *
 *	Feburary, 1998 (version 1.3) Reworked tagtype.c to make it simplier--got rid of two tag on same line functionality
 *	Added ASCII based bolding and italics
 *
 *	March, 1998 (version 1.31)	Made default tag filename <filename>_t.txt
 *	Changed ASCII based bolding and italics
 *
 */
#include <stdlib.h>
#include "htmless.h"

int main(int argc, char* argv[])
{
	FILE *fptr;							/* Pointer to original HTML file */
	FILE *txtfptr;                      /* Pointer to text file */
	FILE *tagfptr;                      /* Pointer to tag file */
	char *htmlfile;           			/* HTML filename */
	char *dotptr;                       /* Pointer to last dot in filename */
	int filesize;						/* The filesize in bytes of default tag and txt files */
	char *textfile=0;           		/* Text filename */
	char *tagfile=0;					/* Text file storing HTML tags */
	char origfile[MAXTAGLENGTH];           /* Holds original filename */
	char c;								/* Next input char */
	char CurrentTag[MAXTAGLENGTH];			/* HTML Tag */
	int space=0;						/* number of spaces printed */
	char specialchar[100];				/* any special HTML characters */
	int NoTagFile=1,NoTextFile=1;       /* Have these filenames been created? */


	/* Print copyright information */
	printf("\nHTMLess (UNIX) HTML-->ASCII Converter Version 1.31\n");
	printf("Copyright(c) 1995-1999. Stephen M. Orth.  All rights reserved.\n");
	printf("HTMLess is free software: you are welcome to redistribute it under the terms of the GNU GENERAL PUBLIC LICENSE.\n\n");

	if (argc <= 1)		   /* quit if no were arguments passed to the function */
	{
		printf("\nUsage: htmless filename.html [<output_textfilename>] [<output_tagfilename>]\n");
		return 0;
	}
	else   	/* Allocate memory for htmlfilename and assign value of argv[1] */
	{
		htmlfile=(char*)malloc(sizeof(argv[1]));
		htmlfile=argv[1];
		if (!checkfile(htmlfile))
			return 0;
	}

	/* Compute default filesize in bytes */
	filesize= (strlen(htmlfile)+4);

	if (argc>=3)  /* if more than two arguments the next must be the textfile name */
	{
		textfile=(char *)malloc(sizeof(argv[2]));
		textfile=argv[2];
		NoTextFile=0;
		if (!checkfile(textfile))
			return 0;
		if (!filexist(textfile))
			return 0;
	}
	else    /* no command line argument given; create space for default filename */
		textfile=(char*)malloc(filesize*sizeof(char));


	if (argc>=4)  /* if more than three arguments the next must be the tagfile name */
	{
		tagfile=(char*)malloc(sizeof(argv[3]));
		tagfile=argv[3];
		NoTagFile=0;
		if (!checkfile(tagfile))
			return 0;
		if (!filexist(tagfile))
			return 0;
	}
	else    /* no command line argument given; create space for default filename */
		tagfile=(char*)malloc(filesize*sizeof(char));


	if ((strrchr(htmlfile,'.'))!=NULL)  		/* If the filename has a dot in it */
		dotptr = strrchr(htmlfile,'.');
	else
		dotptr = &htmlfile[strlen(htmlfile)];	/* Filename has no dot */

	strcpy(origfile,htmlfile);					/* Copy orig. filename to a safe place */

	/*Test if the file is an html file (assume correct extension is a valid HTML file */

	if ((strcmp(dotptr+1,"htm"))==0 || (strcmp(dotptr+1,"HTM"))==0 || (strcmp(dotptr+1,"html"))==0 || (strcmp(dotptr+1,"HTML"))==0)
	{
		*dotptr='\0';

		if (NoTextFile)
		{
			strcpy(textfile,htmlfile);
			strcat(textfile,".txt");
			if (!filexist(textfile))
				return 0;
		}

		if (NoTagFile)
		{
			strcpy(tagfile,htmlfile);
			strcat(tagfile,"_t.txt");
			if (!filexist(tagfile))
				return 0;
		}
		/* If you can open file for reading-exit loop */
		/* Otherwise, notify user and continue */
		if ((fptr=fopen(origfile,"r"))!=NULL)  					
			printf("\n%s opened successfully...\n",origfile);	
		else
		{
			printf("\nCan't open this HTML file. Is your path correct?\n");
			  return 0;
		}
	}
	else     /* else notify user of invalid filename */
	{
		printf("\nNot a valid HTML file.\n");
		return 0;
	}

	if ((txtfptr=fopen(textfile,"w"))==NULL)     /* Create txtfile */
		printf("\nERROR-Unable to create %s\n",textfile);

	if ((tagfptr=fopen(tagfile,"w"))==NULL)      /* Create tagfile */
		printf("\nERROR-Unable to create %s\n",tagfile);

	printf("\nConverting....\n\n");

	/* Begin converting the HTML document into a text and tag file */

	while ((c=getc(fptr))!=EOF)						/*Convert until EOF */
	{
		if (c=='<')									/* It's a tag!!*/
		{
			gettag(CurrentTag,fptr);				/* Get the rest of the tag */
			fprintf(tagfptr,"%s\n",CurrentTag); 	/* Print current tag */
			space=tagtype(CurrentTag,txtfptr);		/* Call TagType to evaluate the new tag read for text file; note spaces*/
		}
		else if (c=='&')							/* It's a special HTML Character */
		{
			getspecial(specialchar,fptr);
			if ((strcmp(specialchar,"& "))==0)
				fprintf(txtfptr,"& ");
			else if ((strcmp(specialchar,"lt"))==0 || (strcmp(specialchar,"#60"))==0 )
				putc('<',txtfptr);
			else if ((strcmp(specialchar,"gt"))==0 || (strcmp(specialchar,"#62"))==0 )
				putc('>',txtfptr);
			else if ((strcmp(specialchar,"amp"))==0 || (strcmp(specialchar,"#38"))==0 )
				putc('&',txtfptr);
			else if ((strcmp(specialchar,"nbsp"))==0) 
				putc(' ',txtfptr);
			else if ((strcmp(specialchar,"quot"))==0 || (strcmp(specialchar,"#34"))==0 )
				putc('\"',txtfptr);
			else if ((strcmp(specialchar,"copy"))==0 || (strcmp(specialchar,"#169"))==0 ) 
				fprintf(txtfptr,"[Copyright]");
			else if ((strcmp(specialchar,"reg"))==0 || (strcmp(specialchar,"#174"))==0 )
				fprintf(txtfptr,"[Registered]");
			else if ((strcmp(specialchar,"trade"))==0 || (strcmp(specialchar,"#153"))==0 )
				fprintf(txtfptr,"[Trademark]");
			else
				printf("Unable to convert special character: &%s;\n", specialchar);
		}
		else  /*It's not a tag!!!!!! */
		{
			if (isspace(c)==0)						/* It's not a whitespace char, or !isspace(c) */
			{
				putc(c,txtfptr);                    /* Write the char to the textfile */
				space=0;

			}
			else									/* It's whitespace */
				if (isspace(c))                     /* Prevents more than one whitespace in between words */
				{									
					space++;
					if (space==1)					/* test how many space encountered so far */
						putc(' ',txtfptr);
				}

		} 											/* end of "it's not a tag" else */

	}  												/* end of while getc() loop */

	printf("File conversion successful.\n\n");
	printf("TEXTFILE = %s\n",textfile);          	/* Inform user of newly created files*/
	printf("TAGFILE = %s\n",tagfile);
	fclose(tagfptr);                                /* Close all open files */
	fclose(txtfptr);
	fclose(fptr);
	return 0;
}													/* end of main() */


