
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "o3read.h"

enum {LIST_NONE=0, LIST_UL, LIST_OL, LIST_DL};

typedef struct hstate {
	int pre;	/* preformatted, default 0 = false */
	int list;	/* list mode, default LIST_NONE */
	int ll;		/* list level, starts with 1 */
	int indent;	/* indentation level, default 0 */
} hstate;

static int href;	/* links, default 0 */
static char **hrefs;

static int nextc(void *closure)
{
	FILE *fp = closure;
	int c = getc(fp);
	if (c == EOF) return '\0';
	return c;
}

static void usage(void)
{
	printf("usage: lmb url\n");
	exit(0);
}

int main(int argc, char **argv)
{
	hnode *h;

	href = 0;
	hrefs = NULL;

	h = parse_html(nextc, stdin);
	if (h == NULL) usage();

	dump_html(h, 0);

	free_html(h);
	return 0;
}

