#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define MAXTAGLENGTH 1000			/* Maximum tag length */
#define OUTWIDTH 80					/* Defines the output width is characters */
#define FILENAMELENGTH 128
#define TRUE 1
#define FALSE 0

/* function declarations */
void gettag (char *string, FILE *fptr);     /* Function to read the HTML tags; takes string ptr of file to read from */
int tagtype (char *string, FILE *txtfptr);  /* Function that formats the text file based ipon each HTML tag; takes string ptr of file to write to*/
int checkfile (char *string);               /* Function to determine if filename is legal */
int filexist (char *string);                /* Checks to see if the file already exists */
int getfname (char *string);                /* Obtain filename from the user */
void getspecial (char *string, FILE *fptr); /* Function to read the special tags; takes string ptr of file to read from */
