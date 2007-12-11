/*
   Copyright (C) 2002  Ulric Eriksson <ulric@siag.nu>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.
 */

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

static void tag_text_p(hnode *, hstate *);
static void tag_unknown(hnode *, hstate *);
static void text(hnode *, hstate *);

static struct {
	char *name;
	void (*action)(hnode *, hstate *);
} tag[] = {
	{"TEXT:P", tag_text_p},
	{NULL, tag_unknown}
};

static hstate *new_hstate(void)
{
	hstate *s = cmalloc(sizeof *s);
	s->pre = 0;
	s->indent = 0;
	s->list = LIST_NONE;
	s->ll = 0;
	return s;
}

static void free_hstate(hstate *s)
{
	free(s);
}

static void newline(hstate *s)
{
	int i;

	putchar('\n');
	for (i = 0; i < s->indent; i++)
		putchar(' ');
}

static void tree(hnode *h, hstate *s)
{
	void (*action)(hnode *, hstate *);
	int i;

	if (h == NULL) return;
	if (h->tag == NULL) {
		text(h, s);
	} else {
		for (i = 0; tag[i].name; i++) {
			if (!strcmp(tag[i].name, h->tag)) break;
		}
		action = tag[i].action;
		(*action)(h, s);
	}
}

/* tag handlers are in alphabetical order so I can find them... */

/* Paragraph break */
static void tag_text_p(hnode *h, hstate *s)
{
	newline(s);
	tree(h->child, s);
	tree(h->next, s);
}

/* Handles tags we don't have handlers for. Ignore the tag, do children */
static void tag_unknown(hnode *h, hstate *s)
{
	/* ignore the tag */
	tree(h->child, s);
	tree(h->next, s);
}

/* Copy text onto screen. Treats whitespace differently if this
   is "preformatted".
*/
static void text(hnode *h, hstate *s)
{
	int i;
	for (i = 0; h->text[i]; i++) {
		putchar(h->text[i]);
	}
	/* can't have children */
	tree(h->next, s);
}

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
	hstate *s;

	href = 0;
	hrefs = NULL;

	h = parse_html(nextc, stdin);
	if (h == NULL) usage();

	s = new_hstate();
	tree(h, s);
	newline(s);

	free_html(h);
	free_hstate(s);
	return 0;
}

