#include <stdio.h>
#include <ctype.h>
typedef unsigned char Boolean;
#define True	1
#define False	0

/*
 * Resource splitter.
 * Returns 1 (success), 0 (EOF), -1 (error).
 */
int
split_resource(st, left, right)
char **st;
char **left;
char **right;
{
	char *s = *st;
	char *t;
	Boolean quote;

	/* Skip leading white space. */
	while (isspace(*s))
		s++;

	/* If nothing left, EOF. */
	if (!*s)
		return 0;

	/* There must be a left-hand side. */
	if (*s == ':')
		return -1;

	/* Scan until an unquoted colon is found. */
	*left = s;
	for (; *s && *s != ':' && *s != '\n'; s++)
		if (*s == '\\' && *(s+1) == ':')
			s++;
	if (*s != ':')
		return -1;

	/* Stip white space before the colon. */
	for (t = s-1; isspace(*t); t--)
		*t = '\0';

	/* Terminate the left-hand side. */
	*(s++) = '\0';

	/* Skip white space after the colon. */
	while (*s != '\n' && isspace(*s))
		s++;

	/* There must be a right-hand side. */
	if (!*s || *s == '\n')
		return -1;

	/* Scan until an unquoted newline is found. */
	*right = s;
	quote = False;
	for (; *s; s++) {
		if (*s == '\\' && *(s+1) == '"')
			s++;
		else if (*s == '"')
			quote = !quote;
		else if (!quote && *s == '\n')
			break;
	}

	/* Strip white space before the newline. */
	if (*s) {
		t = s;
		*st = s+1;
	} else {
		t = s-1;
		*st = s;
	}
	while (isspace(*t))
		*t-- = '\0';

	/* Done. */
	return 1;
}

char *ts = {
	" Macro 1: String(\"He\\\"llo\")               \n Macro 2: String(\"a\n\")\n Macr\\:o 3 : String(\"x\\y\")             "
};

main()
{
	char *s, *left, *right;
	Boolean done = False;

	s = ts;
	while (!done) {
		switch (split_resource(&s, &left, &right)) {
		    case 1:
			printf("Good: '%s' : '%s'\n", left, right);
			break;
		    case -1:
			printf("Error\n");
			done = True;
			break;
		    case 0:
			printf("eof\n");
			done = True;
			break;
		}
	}
}
