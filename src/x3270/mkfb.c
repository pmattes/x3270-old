/*
 * Copyright 1995, 1996, 1999, 2000 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 * mkfb.c
 *	Utility to create fallback definitions from a simple #ifdef'd .ad
 *	file
 */

#include "parts.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define BUFSZ	1024		/* input line buffer size */
#define ARRSZ	8192		/* output array size */
#define SSSZ	10		/* maximum nested ifdef */

unsigned a_color[ARRSZ];	/* array of color indices */
unsigned l_color[ARRSZ];
unsigned n_color = 0;		/* number of color definitions */

unsigned a_mono[ARRSZ];		/* array of mono indices */
unsigned l_mono[ARRSZ];
unsigned n_mono = 0;		/* number of mono definitions */

/* ifdef state stack */
#define MODE_COLOR	0x00000001
#define MODE_FT		0x00000002
#define MODE_TRACE	0x00000004
#define MODE_MENUS	0x00000008
#define MODE_ANSI	0x00000010
#define MODE_KEYPAD	0x00000020
#define MODE_APL	0x00000040
#define MODE_PRINTER	0x00000080

#define MODEMASK	0x0000007f

struct {
	unsigned long ifdefs;
	unsigned long ifndefs;
	unsigned lno;
} ss[SSSZ];
int ssp = 0;

struct {
	char *name;
	unsigned long mask;
} parts[] = {
	{ "COLOR", MODE_COLOR },
	{ "X3270_FT", MODE_FT },
	{ "X3270_TRACE", MODE_TRACE },
	{ "X3270_MENUS", MODE_MENUS },
	{ "X3270_ANSI", MODE_ANSI },
	{ "X3270_KEYPAD", MODE_KEYPAD },
	{ "X3270_APL", MODE_APL },
	{ "X3270_PRINTER", MODE_PRINTER }
};
#define NPARTS	(sizeof(parts)/sizeof(parts[0]))

unsigned long is_defined =
    MODE_COLOR |
#if defined(X3270_FT)
	MODE_FT
#else
	0
#endif
|
#if defined(X3270_TRACE)
	MODE_TRACE
#else
	0
#endif
|
#if defined(X3270_MENUS)
	MODE_MENUS
#else
	0
#endif
|
#if defined(X3270_ANSI)
	MODE_ANSI
#else
	0
#endif
|
#if defined(X3270_KEYPAD)
	MODE_KEYPAD
#else
	0
#endif
|
#if defined(X3270_APL)
	MODE_APL
#else
	0
#endif
|
#if defined(X3270_PRINTER)
	MODE_PRINTER
#else
	0
#endif
    ;
unsigned long is_undefined;

char *me;

void emit(char c);

void
usage(void)
{
	fprintf(stderr, "usage: %s [infile]\n", me);
	exit(1);
}

int
main(int argc, char *argv[])
{
	char buf[BUFSZ];
	int lno = 0;
	int cc = 0;
	int i;
	int continued = 0;
	char *filename = "standard input";

	is_undefined = MODE_COLOR | (~is_defined & MODEMASK);

	/* Parse arguments. */
	if ((me = strrchr(argv[0], '/')) != (char *)NULL)
		me++;
	else
		me = argv[0];
	switch (argc) {
	    case 1:
		break;
	    case 2:
		if (freopen(argv[1], "r", stdin) == (FILE *)NULL) {
			perror(argv[1]);
			exit(1);
		}
		filename = argv[1];
		break;
	    default:
		usage();
	}

	/* Emit the initial boilerplate. */
	printf("/* This file was created automatically from %s by mkfb. */\n\n",
	    filename);
	printf("#if !defined(USE_APP_DEFAULTS) /*[*/\n\n");
	printf("#include <stdio.h>\n");
	printf("#include <X11/Intrinsic.h>\n\n");
	printf("static unsigned char fsd[] = {\n");

	/* Scan the file, emitting the fsd array and creating the indices. */
	while (fgets(buf, BUFSZ, stdin) != (char *)NULL) {
		char *s = buf;
		int sl;
		char c;
		int white;
		int i;
		unsigned long ifdefs;
		unsigned long ifndefs;

		lno++;

		/* Skip leading white space. */
		while (isspace(*s))
			s++;

		/* Remove trailing white space. */
		while ((sl = strlen(s)) && isspace(s[sl-1]))
			s[sl-1] = '\0';

		if (continued)
			goto emit_text;

		/* Skip comments and empty lines. */
		if (!*s || *s == '!')
			continue;

		/* Check for simple if[n]defs. */
		if (*s == '#') {
			int ifnd = 1;

			if (!strncmp(s, "#ifdef ", 7) ||
			    !(ifnd = strncmp(s, "#ifndef ", 8))) {
				char *tk;

				if (ssp >= SSSZ) {
					fprintf(stderr,
					    "%s, line %d: Stack overflow\n",
					    filename, lno);
					exit(1);
				}
				ss[ssp].ifdefs = 0L;
				ss[ssp].ifndefs = 0L;
				ss[ssp].lno = lno;

				tk = s + 7 + !ifnd;
				for (i = 0; i < NPARTS; i++) {
					if (!strcmp(tk, parts[i].name)) {
						if (!ifnd)
							ss[ssp++].ifndefs =
							    parts[i].mask;
						else
							ss[ssp++].ifdefs =
							    parts[i].mask;
						break;
					}
				}
				if (i >= NPARTS) {
					fprintf(stderr,
					    "%s, line %d: Unknown condition\n",
					    filename, lno);
					exit(1);
				}
				continue;
			}

			else if (!strcmp(s, "#else")) {
				unsigned long tmp;

				if (!ssp) {
					fprintf(stderr,
					    "%s, line %d: Missing #if[n]def\n",
					    filename, lno);
					exit(1);
				}
				tmp = ss[ssp-1].ifdefs;
				ss[ssp-1].ifdefs = ss[ssp-1].ifndefs;
				ss[ssp-1].ifndefs = tmp;
			} else if (!strcmp(s, "#endif")) {
				if (!ssp) {
					fprintf(stderr,
					    "%s, line %d: Missing #if[n]def\n",
					    filename, lno);
					exit(1);
				}
				ssp--;
			} else {
				fprintf(stderr,
				    "%s, line %d: Unrecognized # directive\n",
				    filename, lno);
				exit(1);
			}
			continue;
		}

		/* Figure out if there's anything to emit. */

		/* First, look for contradictons. */
		ifdefs = 0;
		ifndefs = 0;
		for (i = 0; i < ssp; i++) {
			ifdefs |= ss[i].ifdefs;
			ifndefs |= ss[i].ifndefs;
		}
		if (ifdefs & ifndefs) {
#if 0
			fprintf(stderr, "contradiction, line %d\n", lno);
#endif
			continue;
		}

		/* Then, apply the actual values. */
		if (ifdefs && (ifdefs & is_defined) != ifdefs) {
#if 0
			fprintf(stderr, "ifdef failed, line %d\n", lno);
#endif
			continue;
		}
		if (ifndefs && (ifndefs & is_undefined) != ifndefs) {
#if 0
			fprintf(stderr, "ifndef failed, line %d\n", lno);
#endif
			continue;
		}

		/* Add array offsets. */
		if (!(ifdefs & MODE_COLOR) && !(ifndefs & MODE_COLOR)) {
			if (n_color >= ARRSZ || n_mono >= ARRSZ) {
				fprintf(stderr,
				    "%s, line %d: Buffer overflow\n",
				    filename, lno);
				exit(1);
			}
			a_color[n_color] = cc;
			l_color[n_color++] = lno;
			a_mono[n_mono] = cc;
			l_mono[n_mono++] = lno;
		} else if (ifdefs & MODE_COLOR) {
			if (n_color >= ARRSZ) {
				fprintf(stderr,
				    "%s, line %d: Buffer overflow\n",
				    filename, lno);
				exit(1);
			}
			a_color[n_color] = cc;
			l_color[n_color++] = lno;
		} else {
			if (n_mono >= ARRSZ) {
				fprintf(stderr,
				    "%s, line %d: Buffer overflow\n",
				    filename, lno);
				exit(1);
			}
			a_mono[n_mono] = cc;
			l_mono[n_mono++] = lno;
		}

		/* Emit the text. */
	    emit_text:
		continued = 0;
		white = 0;
		while (c = *s++) {
			if (c == ' ' || c == '\t')
				white++;
			else if (white) {
				emit(' ');
				cc++;
				white = 0;
			}
			switch (c) {
			    case ' ':
			    case '\t':
				break;
			    case '#':
				emit('\\');
				emit('#');
				cc += 2;
				break;
			    case '\\':
				if (*s == '\0') {
					continued = 1;
					break;
				}
				/* else fall through */
			    default:
				emit(c);
				cc++;
				break;
			}
		}
		if (white) {
			emit(' ');
			cc++;
			white = 0;
		}
		if (!continued) {
			emit(0);
			cc++;
		}
	}
	if (ssp) {
		fprintf(stderr, "%d missing #endif(s) in %s\n", ssp, filename);
		fprintf(stderr, "last #ifdef was at line %u\n", ss[ssp-1].lno);
		exit(1);
	}
	printf("};\n\n");

	/* Emit the fallback arrays themselves. */
	printf("String color_fallbacks[%u] = {\n", n_color + 1);
	for (i = 0; i < n_color; i++)
		printf("\t(String)&fsd[%u], /* line %u */\n", a_color[i],
		    l_color[i]);
	printf("\t(String)NULL\n};\n\n");
	printf("String mono_fallbacks[%u] = {\n", n_mono + 1);
	for (i = 0; i < n_mono; i++)
		printf("\t(String)&fsd[%u], /* line %u */\n", a_mono[i],
		    l_mono[i]);
	printf("\t(String)NULL\n};\n\n");

	/* Emit some test code. */
	printf("%s", "#if defined(DEBUG) /*[*/\n\
main()\n\
{\n\
	int i;\n\
\n\
	for (i = 0; color_fallbacks[i]; i++)\n\
		printf(\"color %d: %s\\n\", i, color_fallbacks[i]);\n\
	for (i = 0; mono_fallbacks[i]; i++)\n\
		printf(\"mono %d: %s\\n\", i, mono_fallbacks[i]);\n\
	exit(0);\n\
}\n");
	printf("#endif /*]*/\n\n");
	printf("#endif /*]*/\n");

	return 0;
}

int n_out = 0;

void
emit(char c)
{
	if (n_out >= 19) {
		printf("\n");
		n_out = 0;
	}
	printf("%3d,", (unsigned char)c);
	n_out++;
}
