/*
 * Copyright 1997, 1999, 2000, 2001 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 * Quick C preprocessor substitute, for converting X3270.ad to X3270.ad.
 *
 * Understands #if[n]def COLOR, and does the right thing.  All other #ifdefs
 * are assumed to be true.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_NEST	50

extern char *optarg;
extern int optind;

char *me;
int color = 0;

void
usage(void)
{
	fprintf(stderr, "usage: %s [-DCOLOR] [-UCOLOR] [infile [outfile]]\n",
	    me);
	exit(1);
}

int
main(int argc, char *argv[])
{
	int c;
	char buf[1024];
	FILE *f, *t, *o;
	int nest = 0;
	int ln = 0;
	int pass[MAX_NEST];
	int elsed[MAX_NEST];

	if ((me = strrchr(argv[0], '/')) != (char *)NULL)
		me++;
	else
		me = argv[0];

	while ((c = getopt(argc, argv, "D:U:")) != -1) {
		switch (c) {
		    case 'D':
			if (strcmp(optarg, "COLOR")) {
				fprintf(stderr, "only -DCOLOR is supported\n");
				exit(1);
			}
			color = 1;
			break;
		    case 'U':
			if (strcmp(optarg, "COLOR")) {
				fprintf(stderr, "only -UCOLOR is supported\n");
				exit(1);
			}
			color = 0;
			break;
		    default:
			usage();
			break;
		}
	}
	switch (argc - optind) {
	    case 0:
		f = stdin;
		break;
	    case 1:
	    case 2:
		if (strcmp(argv[optind], "-")) {
			f = fopen(argv[optind], "r");
			if (f == (FILE *)NULL) {
				perror(argv[optind]);
				exit(1);
			}
		} else
			f = stdin;
		break;
	    default:
		usage();
		break;
	}

	t = tmpfile();
	if (t == NULL) {
		perror("tmpfile");
		exit(1);
	}

	pass[nest] = 1;

	while (fgets(buf, sizeof(buf), f) != (char *)NULL) {
		ln++;
		if (buf[0] != '#') {
			if (pass[nest])
				fprintf(t, "%s", buf);
			continue;
		}
		if (!strcmp(buf, "#ifdef COLOR\n")) {
			pass[nest+1] = pass[nest] && color;
			nest++;
			elsed[nest] = 0;
		} else if (!strncmp(buf, "#ifdef ", 7)) {
			pass[nest+1] = pass[nest];
			nest++;
			elsed[nest] = 0;
		} else if (!strcmp(buf, "#ifndef COLOR\n")) {
			pass[nest+1] = pass[nest] && !color;
			nest++;
			elsed[nest] = 0;
		} else if (!strncmp(buf, "#ifndef ", 8)) {
			pass[nest+1] = 0;
			nest++;
			elsed[nest] = 0;
		} else if (!strcmp(buf, "#else\n")) {
			if (!nest) {
				fprintf(stderr, "line %d: #else without #if\n",
				    ln);
				exit(1);
			}
			if (elsed[nest]) {
				fprintf(stderr, "line %d: duplicate #else\n",
				    ln);
				exit(1);
			}
			if (pass[nest])
				pass[nest] = 0;
			else if (pass[nest-1])
				pass[nest] = 1;
			elsed[nest] = 1;
		} else if (!strcmp(buf, "#endif\n")) {
			if (!nest) {
				fprintf(stderr, "line %d: #endif without #if\n",
				    ln);
				exit(1);
			}
			--nest;
		} else {
			fprintf(stderr, "line %d: unknown directive\n", ln);
			exit(1);
		}
#if 0
		fprintf(t, "! line %d nest %d pass[nest] %d\n",
		    ln, nest, pass[nest]);
#endif
	}
	if (nest > 0) {
		fprintf(stderr, "missing #endif\n");
		exit(1);
	}

	/* Close the input file, if there was one. */
	if (f != stdin)
		fclose(f);

	/* Open the output file, if there is one. */
	if (argc - optind == 2) {
		o = fopen(argv[optind + 1], "w");
		if (o == NULL) {
			perror(argv[optind + 1]);
			exit(1);
		}
	} else {
		o = stdout;
	}

	/* Copy the temp file to the output file. */
	rewind(t);
	while (fgets(buf, sizeof(buf), t) != NULL) {
		fprintf(o, "%s", buf);
	}
	fclose(t);
	if (o != stdout)
		fclose(o);

	return 0;
}
