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

/* Function to verify if a file already exists.  Returns true if it doesn't exist */
/* or if it's ok to overwrite the existing file */
int filexist (char *string)              	/* Function takes filename */
{
	FILE *fileptr;                        	/* Pointer to filename */
	char overwrite;                       	/* Yes or no for overwrite */


		return TRUE;                       		/* File doesn't exist */
}

/* 	Function to get file name (or any string) from user using getchar()
*	Remember to use malloc in function main to allocate memory when passing
* 	this function a string pointer (i.e. char *myString); not needed if passing char myString[45].
*	This function created 5/3/96.  It reads strings unsing getchar.  Sort of like scanf, but better.
*	Call the function like so: getfname(myString);
*	The function return the number of characters read. Neat-o!
*/
int getfname (char *string)
{
	int c;
	int i=0;


	c=getchar();                   /* get rid of first whitespace char */
	if (isspace(c))
		;
	else                           /* If it's not whitespace, add it to string */
		string[i++]= c;

	while ((c=getchar())!='\n')    /* Go until maxlength */
		if (i<FILENAMELENGTH)
			string[i++]= c;
											 /* Make the array of chars a string */
	string[i++]='\0';

	return strlen(string);
}

int checkfile (char *string)                      /* Function checks validity of filename */
{
	int length=0,x;
																  /* Determine length of input string */
	length=strlen(string);                         /* DOS Test.  No filenames > 8 chars */

	if (length<=0)                                 /* If the file is of length 0 */
	{
		printf("\nYou must enter a filename.\n");
		return FALSE;
	}
	if (length > FILENAMELENGTH)
	{
		printf("\nYou have exceeded the maximum pathlength of %d characters\n",FILENAMELENGTH);
		return FALSE;                   
	}

	for (x=0;x<length;x++)      	 /* Walk through each char of the string */
	{
		switch (string[x])          /* Unacceptabel for both Win95 and 8.3 filenames */
		{
/*			case '\\':
         			printf("\n'%c' is not a legal filename character.\n",string[x]);
				return FALSE;  */
			case '/':
				printf("\n'%c' is not a legal filename character.\n",string[x]);
				return FALSE;
/*			case ':':
				printf("\n'%c' is not a legal filename character.\n",string[x]);
				return FALSE;
			case '*':
				printf("\n'%c' is not a legal filename character.\n",string[x]);
				return FALSE;
			case '?':
				printf("\n'%c' is not a legal filename character.\n",string[x]);
				return FALSE;    */
			case '"':
				printf("\n'%c' is not a legal filename character.\n",string[x]);
				return FALSE;
/*			case '<':
				printf("\n'%c' is not a legal filename character.\n",string[x]);
				return FALSE;
			case '>':
				printf("\n'%c' is not a legal filename character.\n",string[x]);
				return FALSE;
			case '|':
				printf("\n'%c' is not a legal filename character.\n",string[x]);
				return FALSE;  */
			case '	':                               /* Tab character */
				printf("\n'%c' is not a legal filename character.\n",string[x]);
				return FALSE;    
            	/* Win 95 can handle all these below here */
/*			case '.':
				printf("\n'%c' is not a legal filename character.\n",string[x]);
				return FALSE;   */
			case ' ':
				printf("\n'%c' is not a legal filename character.\n",string[x]);
				return FALSE;
			case ',':
				printf("\n'%c' is not a legal filename character.\n",string[x]);
				return FALSE;
			case ';':
				printf("\n'%c' is not a legal filename character.\n",string[x]);
				return FALSE;
			case '[':
				printf("\n'%c' is not a legal filename character.\n",string[x]);
				return FALSE;
			case ']':
				printf("\n'%c' is not a legal filename character.\n",string[x]);
				return FALSE;
			case '+':
				printf("\n'%c' is not a legal filename character.\n",string[x]);
				return FALSE;
			case '=':
				printf("\n'%c' is not a legal filename character.\n",string[x]);
				return FALSE;
		}
	}
	return TRUE;                        
}
