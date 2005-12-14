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
 */
#include "htmless.h"

/* Function that pulls HTML tags from text document */
void gettag (char *string,FILE *fptr)              /* Takes empty tag string and filepointer */
{
	int c;														/* Next character read */

	*string++='<';                                  /* Replace lost HTML tag marker */
	while ((c=getc(fptr))!='>')                     /* Read until you reach the HTML tag end char */
	{
		*string++= c;                                /* Increment the string ptr*/
	}
	*string++='>';                                  /* Write the end tag char */
	*string='\0';                                   /* Make it a string */
	return;
}

void getspecial (char *string,FILE *fptr)              /* Takes empty tag string and filepointer */
{
	int c;														/* Next character read */

	while ((c=getc(fptr))!=';')                     /* Read until you reach the end of the special char */
	{	
		if (c==' ') 
		{
			*string++='&';
			*string++=' ';
			break;
		}
		else
			*string++= c;                   /* Increment the string ptr*/
	}
	*string='\0';                                   /* Make it a string */
	return;
}

int tagtype (char *string, FILE *txtfptr)          /* Based on the tag type; write various format chars*/
{
	int space=0;                                    /* Count number of spaces */
	int x;                                          /* column width loop counter */
	int line=0;

	if (line==1)                                    /* If we start a new line */
	{
		putc('\n',txtfptr);
		line=0;
	}
	/* The remaining section is fairly self explanitory--just determine which tag
	 * has just been read and then write the appropriate formatting chars to the text file
	 * Assumptions:  	
	 *		The HTML is either all lower case or all upper.
	 *              The program should ignore all whitespace within the HTML file;
	 *				With the exception: the default is one whitespace char in between words.
	 *				Program does not try to evaluate every possible HTML tag; just enough to produce a readable textfile
	 *              Program covers through the HTML 2.0 spec.
	 *
	 *				Added '{' and '}' for Italics and '[' and ']' for bold
	 */

	if ((strcmp(string,"<HTML>"))==0 || (strcmp(string,"<html>"))==0)
		space++;
	if ((strcmp(string,"<BODY>"))==0 || (strcmp(string,"<body>"))==0)
		space++;
	if ((strcmp(string,"</HEAD>"))==0 || (strcmp(string,"</head>"))==0)
	{
		putc('\n',txtfptr);
		space++;
	}
	if ((strcmp(string,"<CENTER>"))==0 || (strcmp(string,"<center>"))==0)
	{
		putc('\n',txtfptr);
		space++;
	}
	if ((strcmp(string,"</CENTER>"))==0 || (strcmp(string,"</center>"))==0)
	{
		putc('\n',txtfptr);
		space++;
	}
	if ((strcmp(string,"<P>"))==0 ||(strcmp(string,"<p>"))==0)        /* End of paragraph tag include a trailing blank line */
	{
		fprintf(txtfptr,"\n\n");
		space--;
	}
	if ((strcmp(string,"<BR>"))==0 ||(strcmp(string,"<br>"))==0)      /* End of line */
	{
		putc('\n',txtfptr);
		space--;
	}
	if ((strncmp(string,"<H",2))==0 || (strncmp(string,"<h",2))==0)   /* A test for the header tag i.e <H3> */
	{
		if (strlen(string)==4)
			putc('\n',txtfptr);
		space++;
	}
	if ((strncmp(string,"</H",3))==0 || (strncmp(string,"</h",3))==0) /*Test for end-header */
	{
		if (strlen(string)==5)
			putc('\n',txtfptr);
		space++;
	}
	if ((strcmp(string,"<DL>"))==0 || (strcmp(string,"<dl>"))==0)     /* A description list */
		space++;
	if ((strcmp(string,"</DL>"))==0 || (strcmp(string,"</dl>"))==0)
	{
		putc('\n',txtfptr);
		space++;
	}
	if ((strcmp(string,"<DD>"))==0 || (strcmp(string,"<dd>"))==0)     /* data definition; indent */
	{
		putc('\n',txtfptr);
		fprintf(txtfptr,"\t\t");
	}
	if ((strcmp(string,"<DT>"))==0 || (strcmp(string,"<dt>"))==0)     /* data term; indent a little */
	{
		fprintf(txtfptr,"\t");
		line=1;
	}
	if ((strcmp(string,"<LI>"))==0 || (strcmp(string,"<li>"))==0)     /* A List item; indent */
	{
      		putc('\n',txtfptr);
		fprintf(txtfptr,"\t\t");

	}
	if ((strcmp(string,"<UL>"))==0 || (strcmp(string,"<ul>"))==0)     /* A unordered list */
		space++;
	if ((strcmp(string,"</UL>"))==0 || (strcmp(string,"</ul>"))==0)
	{
		putc('\n',txtfptr);
		space++;
	}
	if ((strcmp(string,"<OL>"))==0 || (strcmp(string,"<ol>"))==0)     /* A numbered list */
		space++;
	if ((strcmp(string,"</OL>"))==0 || (strcmp(string,"</ol>"))==0)
	{
		putc('\n',txtfptr);
		space++;
	}
	if ((strcmp(string,"<TABLE>"))==0 || (strcmp(string,"<table>"))==0) /* A table */
	{
		putc('\n',txtfptr);
		space++;
	}
	if ((strcmp(string,"</TABLE>"))==0 || (strcmp(string,"</table>"))==0)
	{
		putc('\n',txtfptr);
		space++;
	}
	if ((strncmp(string,"<TH",3))==0 || (strncmp(string,"<th",3))==0)   /* A table header */
	{
		putc('\n',txtfptr);
		fprintf(txtfptr,"\t");
		space++;
	}
	if ((strcmp(string,"</TH>"))==0 || (strcmp(string,"</th>"))==0)
	{
		putc('\n',txtfptr);
		space++;
	}
	if ((strncmp(string,"<TR",3))==0 || (strncmp(string,"<tr",3))==0)   /* A table row */
	{
		fprintf(txtfptr,"\t");
		space++;
	}
	if ((strcmp(string,"</TR>"))==0 || (strcmp(string,"</tr>"))==0)
	{
		putc('\n',txtfptr);
		space++;
	}
	if ((strcmp(string,"<BLINK>"))==0 || (strcmp(string,"<blink>"))==0)
		space++;
	if ((strcmp(string,"</BLINK>"))==0 || (strcmp(string,"</blink>"))==0)
		space++;
	if ((strncmp(string,"<HR",3))==0 || (strncmp(string,"<hr",3))==0)   /* A line; print to max. output width */
	{
		for (x=0;x<OUTWIDTH;x++)
		{
			fprintf(txtfptr,"_");
		}
		fprintf(txtfptr,"\n\n");
		space++;
	}
	if ((strncmp(string,"<IMG",4))==0 || (strncmp(string,"<img",4))==0)
		space++;
	if ((strncmp(string,"<A HREF",7))==0 || (strncmp(string,"<A HREF",7))==0)
		space++;
	if ((strncmp(string,"<FORM",5))==0 || (strncmp(string,"<form",5))==0)
		space++;
	if ((strncmp(string,"</FORM",6))==0 || (strncmp(string,"</form",6))==0)
	{
		putc('\n',txtfptr);
		space++;
	}
	if ((strcmp(string,"<B>"))==0 || (strcmp(string,"<b>"))==0)
	{
		fprintf(txtfptr,"[");
		space++;
	}
	if ((strcmp(string,"</B>"))==0 || (strcmp(string,"</b>"))==0)
	{
		fprintf(txtfptr,"]");
		space++;
	}
	if ((strcmp(string,"<I>"))==0 || (strcmp(string,"<i>"))==0)
	{
		putc('{',txtfptr);
		space++;
	}
	if ((strcmp(string,"</I>"))==0 || (strcmp(string,"</i>"))==0)
	{
		putc('}',txtfptr);
		space++;
	}

	return space;                                      					  /* return the number of spaces encountered */
}
