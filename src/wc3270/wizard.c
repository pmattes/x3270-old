/*
 * Copyright 2006, 2007 by Paul Mattes.
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted,
 *   provided that the above copyright notice appear in all copies and that
 *   both that copyright notice and this permission notice appear in
 *   supporting documentation.
 *
 * wc3270 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the file LICENSE for more details.
 */

/*
 *	wizard.c
 *		A Windows console-based 3270 Terminal Emulator
 *		Session creation wizard
 */

#include "globals.h"
#include <signal.h>
#include "appres.h"
#include "3270ds.h"
#include "resources.h"
#include "ctlr.h"

#include "actionsc.h"
#include "ctlrc.h"
#include "hostc.h"
#include "keymapc.h"
#include "kybdc.h"
#include "macrosc.h"
#include "screenc.h"
#include "tablesc.h"
#include "trace_dsc.h"
#include "utilc.h"
#include "widec.h"
#include "xioc.h"

#include <windows.h>
#include <wincon.h>
#include <shlobj.h>
#include "shlobj_missing.h"

#define STR_SIZE	256
#define LEGAL_CNAME	"ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
			"abcedfghijklmnopqrstuvwxyz" \
			"0123456789_- "

#define KEYMAP_SUFFIX	".wc3270km"
#define KS_LEN		strlen(KEYMAP_SUFFIX)

#define KM_3270		".3270"
#define LEN_3270	strlen(KM_3270)

#define KM_NVT		".nvt"
#define LEN_NVT		strlen(KM_NVT)

extern char *wversion;

char *charsets[] = {
	"belgian",
	"belgian-euro",
	"bracket",
	"brazilian",
	"cp870",
	"finnish",
	"finnish-euro",
	"french",
	"french-euro",
	"german",
	"german-euro",
	"greek",
	"hebrew",
	"icelandic",
	"icelandic-euro",
	"italian",
	"italian-euro",
	"norwegian",
	"norwegian-euro",
	"oldibm",
	"russian",
	"thai",
	"turkish",
	"uk",
	"uk-euro",
	"us",
	"us-euro",
	"us-intl",
	NULL
};
#define CS_WIDTH	18
#define	CS_COLS		(79 / CS_WIDTH)

             /*  model  2   3   4   5 */
int wrows[6] = { 0, 0, 25, 33, 44, 28  };
int wcols[6] = { 0, 0, 80, 80, 80, 132 };

#define MAX_PRINTERS	256
PRINTER_INFO_1 printer_info[MAX_PRINTERS];
int num_printers = 0;
char default_printer[1024];

static struct {
    	char *name;
	char *description;
} builtin_keymaps[] = {
	{ "rctrl",	"Map PC Right Ctrl key to 3270 'Enter' and PC Enter key to 3270 'Newline'" },
	{ NULL,		NULL }
};

typedef struct {
	char session[STR_SIZE];		/* session name */
	char path[MAX_PATH];		/* path to session file */
	char host[STR_SIZE];		/* host name */
	int  port;			/* TCP port */
	char luname[STR_SIZE];		/* LU name */
	int  ssl;			/* SSL tunnel flag */
	int  model;			/* model number */
	char charset[STR_SIZE];		/* character set name */
	int  wpr3287;			/* wpr3287 flag */
	char printerlu[STR_SIZE];	/*  printer LU */
	char printer[STR_SIZE];		/*  Windows printer name */
	char keymaps[STR_SIZE];		/* keymap names */
} session_t;

int create_session_file(session_t *s);


// CreateLink - uses the shell's IShellLink and IPersistFile interfaces 
//   to create and store a shortcut to the specified object. 
// Returns the result of calling the member functions of the interfaces. 
// lpszPathObj - address of a buffer containing the path of the object 
// lpszPathLink - address of a buffer containing the path where the 
//   shell link is to be stored 
// lpszDesc - address of a buffer containing the description of the 
//   shell link 
HRESULT
CreateLink(LPCSTR lpszPathObj, LPSTR lpszPathLink, LPSTR lpszDesc,
    LPSTR lpszArgs, LPSTR lpszDir, int rows, int cols)
{
	HRESULT			hres;
	int	 		initialized;
	IShellLink*		psl = NULL; 
	IShellLinkDataList* 	psldl = NULL; 
	IPersistFile*		ppf = NULL;
	NT_CONSOLE_PROPS	p;
	WORD			wsz[MAX_PATH];

	hres = CoInitialize(NULL);
	if (!SUCCEEDED(hres))
		goto out;
	initialized = 1;

	// Get a pointer to the IShellLink interface.
	hres = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
	    &IID_IShellLink, (LPVOID *)&psl);

	if (!SUCCEEDED(hres))
		goto out;

	// Set the path to the shortcut target, and add the description.
	psl->lpVtbl->SetPath(psl, lpszPathObj);
	psl->lpVtbl->SetDescription(psl, lpszDesc);
	if (lpszArgs)
	    psl->lpVtbl->SetArguments(psl, lpszArgs);
	if (lpszDir)
	    psl->lpVtbl->SetWorkingDirectory(psl, lpszDir);

	hres = psl->lpVtbl->QueryInterface(psl, &IID_IShellLinkDataList,
	    (void **)&psldl);
	if (!SUCCEEDED(hres))
		goto out;

	memset(&p, '\0', sizeof(NT_CONSOLE_PROPS));
	p.dbh.cbSize = sizeof(p);
	p.dbh.dwSignature = NT_CONSOLE_PROPS_SIG;
	p.wFillAttribute = 7;	/* ? */
	p.wPopupFillAttribute = 245;	/* ? */
	p.dwScreenBufferSize.X = cols;
	p.dwScreenBufferSize.Y = 0x012c;
	p.dwWindowSize.X = cols;
	p.dwWindowSize.Y = rows;
	p.dwWindowOrigin.X = 0;
	p.dwWindowOrigin.Y = 0;
	p.nFont = 0;
	p.nInputBufferSize = 0;
	p.dwFontSize.X = 0;
	p.dwFontSize.Y = 0;
	p.uFontFamily = 54; /* ? */
	p.uFontWeight = 400; /* ? */
	wcscpy(p.FaceName, L"Lucida Console");
	p.uCursorSize = 0x19;
	p.bFullScreen = 0;
	p.bQuickEdit = 0;
	p.bInsertMode = 1;
	p.bAutoPosition = 1;
	p.uHistoryBufferSize = 0x32;
	p.uNumberOfHistoryBuffers = 4;
	p.bHistoryNoDup = 0;
	p.ColorTable[0] = 0;
	p.ColorTable[1] =  0x00800000;
	p.ColorTable[2] =  0x00008000;
	p.ColorTable[3] =  0x00808000;
	p.ColorTable[4] =  0x00000080;
	p.ColorTable[5] =  0x00800080;
	p.ColorTable[6] =  0x00008080;
	p.ColorTable[7] =  0x00c0c0c0;
	p.ColorTable[8] =  0x00808080;
	p.ColorTable[9] =  0x00ff0000;
	p.ColorTable[10] = 0x0000ff00;
	p.ColorTable[11] = 0x00ffff00;
	p.ColorTable[12] = 0x000000ff;
	p.ColorTable[13] = 0x00ff00ff;
	p.ColorTable[14] = 0x0000ffff;
	p.ColorTable[15] = 0x00ffffff;

	hres = psldl->lpVtbl->AddDataBlock(psldl, &p);
	if (!SUCCEEDED(hres))
		goto out;

	// Query IShellLink for the IPersistFile interface for saving
	// the shortcut in persistent storage.
	hres = psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile,
	    (void **)&ppf);

	if (!SUCCEEDED(hres))
		goto out;

	// Ensure that the string is ANSI.
	MultiByteToWideChar(CP_ACP, 0, lpszPathLink, -1, wsz, MAX_PATH);

	// Save the link by calling IPersistFile::Save.
	hres = ppf->lpVtbl->Save(ppf, wsz, TRUE);

out:
	if (ppf != NULL)
		ppf->lpVtbl->Release(ppf);
	if (psldl != NULL)
		psldl->lpVtbl->Release(psldl);
	if (psl != NULL)
		psl->lpVtbl->Release(psl);

	if (initialized)
		CoUninitialize();

	return hres;
} 

char *
get_input(char *buf, int bufsize)
{
    	char *s;
	int sl;

	/* Get the raw input from stdin. */
	if (fgets(buf, bufsize, stdin) == NULL)
	    	return NULL;

	/* Trim leading whitespace. */
	s = buf;
	sl = strlen(buf);
	while (*s && isspace(*s)) {
		s++;
		sl--;
	}
	if (s != buf)
		memmove(buf, s, sl + 1);

	/* Trim trailing whitespace. */
	while (sl && isspace(buf[--sl]))
		buf[sl] = '\0';

	return buf;
}

int
getyn(int defval)
{
	char yn[STR_SIZE];

	if (get_input(yn, STR_SIZE) == NULL) {
		return -1;
	}

	if (!yn[0])
		return defval;

	if (!strncasecmp(yn, "yes", strlen(yn)))
		return 1;
	if (!strncasecmp(yn, "no", strlen(yn)))
		return 0;

	printf("Please answer (y)es or (n)o.\n\n");
	return -2;
}

void
enum_printers(void)
{
	DWORD needed = 0;
	DWORD returned = 0;

	/* Get the default printer name. */
	default_printer[0] = '\0';
	if (GetProfileString("windows", "device", "", default_printer,
		    sizeof(default_printer)) != 0) {
		char *comma;

		if ((comma = strchr(default_printer, ',')) != NULL)
			*comma = '\0';
	}

	/* Get the list of printers. */
	if (EnumPrinters(
		    PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS,
		    NULL,
		    1,
		    (LPBYTE)&printer_info,
		    sizeof(printer_info),
		    &needed,
		    &returned) == 0)
	    return;

	num_printers = returned;
}

/* Get an 'other' printer name. */
int
get_printer_name(char *printername)
{
	for (;;) {
		printf("\nEnter Windows printer name: [use system default] ");
		fflush(stdout);
		if (get_input(printername, STR_SIZE) == NULL)
			return -1;
		if (!printername[0])
			break;
		if (strchr(printername, '!') ||
		    strchr(printername, ',')) {
			printf("Invalid printer name.\n");
			continue;
		} else {
			break;
		}
	}
	return 0;
}

typedef struct km {
	struct km *next;
    	char name[MAX_PATH];
} km_t;
km_t *km_first = NULL;
km_t *km_last = NULL;

/* Save a keymap name.  If it is unique, return its stem. */
char *
save_keymap_name(char *keymap_name)
{
	km_t *km;
    	int sl;
	km_t *kms;

	km = (km_t *)malloc(sizeof(km_t));
	if (km == NULL) {
	    	fprintf(stderr, "Not enough memory\n");
		return NULL;
	}
	strcpy(km->name, keymap_name);
    	sl = strlen(km->name);

	if (sl > KS_LEN && !strcasecmp(km->name + sl - KS_LEN, KEYMAP_SUFFIX)) {
		km->name[sl - KS_LEN] = '\0';
		sl -= KS_LEN;
	}
	if (sl > LEN_3270 && !strcasecmp(km->name + sl - LEN_3270, KM_3270)) {
		km->name[sl - LEN_3270] = '\0';
		sl -= LEN_3270;
	} else if (sl > LEN_NVT &&
		    !strcasecmp(km->name + sl - LEN_NVT, KM_NVT)) {
		km->name[sl - LEN_NVT] = '\0';
		sl -= LEN_NVT;
	}

	for (kms = km_first; kms != NULL; kms = kms->next) {
	    	if (!strcasecmp(kms->name, km->name))
		    	break;
	}
	if (kms != NULL) {
	    	free(km);
		return NULL;
	}
	km->next = NULL;
	if (km_last != NULL)
	    	km_last->next = km;
	else
	    	km_first = km;
	km_last = km;
	return km->name;
}

void
new_screen(session_t *s, char *title)
{
    	system("cls");
	printf(
"wc3270 Session Wizard                                            %s\n",
		wversion);
	if (s->session[0])
	    	printf("\nSession: %s\n", s->session);
	printf("\n%s\n", title);
}

/* Introductory screen. */
int
intro(session_t *s)
{
	int rc;

	new_screen(s, "\
Overview\n\
\n\
This wizard sets up a new wc3270 session.\n\
\n\
It creates a session file in the wc3270 installation directory and a\n\
shortcut on your desktop.");

	for (;;) {
		printf("\nContinue? (y/n) [y] ");
		fflush(stdout);
		rc = getyn(1);
		if (rc == -1 || rc == 0)
			return -1;
		if (rc == 1)
			break;
	}
	return 0;
}

/* Session name screen. */
int
get_session(session_t *s, char *installdir)
{
    	FILE *f;
	int rc;

	/* Get the session name. */
	new_screen(s, "\
Session Name\n\
\n\
This is a unique name for the wc3270 session.  It is the name of the file\n\
containing the session configuration parameters and the name of the desktop\n\
link.");
	for (;;) {
		printf("\nEnter session name: ");
		fflush(stdout);
		if (get_input(s->session, sizeof(s->session)) == NULL) {
			return -1;
		}
		if (!s->session[0])
			continue;
		if (strspn(s->session, LEGAL_CNAME) != strlen(s->session)) {
			fprintf(stdout, "\
\nIllegal character(s).\n\
Session names can only have letters, numbers, spaces, underscore '_'\n\
and dash '-')\n");
			continue;
		}

		break;
	}
	sprintf(s->path, "%s\\%s.wc3270", installdir, s->session);
	f = fopen(s->path, "r");
	if (f != NULL) {
		for (;;) {
			printf("\nSession '%s' already exists.  "
			    "Overwrite it? (y/n) [n] ", s->session);
			fflush(stdout);
			rc = getyn(0);
			if (rc == -1)
				return -1;
			if (rc == 0)
			    	return -2;
			if (rc == 1)
				break;
		}
		fclose(f);
	}
	return 0;
}

int
get_host(session_t *s)
{
	new_screen(s, "\
Host Name\n\
\n\
This specifies the IBM host to connect to.  It can be a symbolic name like\n\
'foo.company.com', an IPv4 address in dotted-decimal notation such as\n\
'1.2.3.4', or an IPv6 address in colon notation, such as 'fec0:0:0:1::27'.");

	for (;;) {
		if (strchr(s->session, ' ') == NULL)
			printf("\nEnter host name or IP address: [%s] ",
				s->session);
		else
			printf("\nEnter host name or IP address: ");
		fflush(stdout);
		if (get_input(s->host, sizeof(s->host)) == NULL) {
			return -1;
		}
		if (strchr(s->host, ' ') != NULL) {
			printf("\nHost names cannot contain spaces.\n");
			continue;
		}
		if (strchr(s->host, '@') != NULL) {
			printf("\nHostnames cannot contain '@' characters.\n");
			continue;
		}
		if (strchr(s->host, '[') != NULL) {
			printf("\nHostnames cannot contain '[' characters.\n");
			continue;
		}
		if (strchr(s->host, ']') != NULL) {
			printf("\nHostnames cannot contain ']' characters.\n");
			continue;
		}
		if (!s->host[0]) {
			if (strchr(s->session, ' ') != NULL)
				continue;
			strcpy(s->host, s->session);
		}
		break;
	}
	return 0;
}

int
get_port(session_t *s)
{
    	char inbuf[STR_SIZE];
	char *ptr;
	unsigned long u;

    	new_screen(s, "\
TCP Port\n\
\n\
This specifies the TCP Port to use to connect to the host.  It is a number from\n\
1 to 65535 or the name 'telnet'.  The default is the 'telnet' port, port 23.");

	s->port = 23;
	for (;;) {
		printf("\nTCP port: [telnet] ");
		if (get_input(inbuf, sizeof(inbuf)) == NULL) {
			return -1;
		}
		if (!inbuf[0])
			break;
		if (!strcasecmp(inbuf, "telnet")) {
			break;
		}
		u = strtoul(inbuf, &ptr, 10);
		if (u < 1 || u > 65535 || *ptr != '\0') {
			printf("Invalid port.\n");
		} else {
		    	s->port = (int)u;
			break;
		}
	}
	return 0;
}

int
get_lu(session_t *s)
{
    	new_screen(s, "\
Logical Unit (LU) Name\n\
\n\
This specifies a particular Logical Unit or Logical Unit group to connect to\n\
on the host.  The default is to allow the host to select the Logical Unit.");

	for (;;) {
		printf("\nEnter Logical Unit (LU) name: [none] ");
		fflush(stdout);
		if (get_input(s->luname, sizeof(s->luname)) == NULL) {
			return -1;
		}
		if (strchr(s->luname, ':') != NULL) {
		    	printf("\nLU name cannot contain ':' characters.\n");
			continue;
		}
		if (strchr(s->luname, '@') != NULL) {
		    	printf("\nLU name cannot contain '@' characters.\n");
			continue;
		}
		if (strchr(s->luname, '[') != NULL) {
		    	printf("\nLU name cannot contain '[' characters.\n");
			continue;
		}
		if (strchr(s->luname, ']') != NULL) {
		    	printf("\nLU name cannot contain ']' characters.\n");
			continue;
		}
		break;
	}
	return 0;
}

int
get_model(session_t *s)
{
	int i;
    	char inbuf[STR_SIZE];
	char *ptr;
	unsigned long u;

	new_screen(s, "\
Model Number\n\
\n\
This specifies the dimensions of the screen.");

	s->model = 4;
	printf("\n");
	for (i = 2; i <= 5; i++) {
		if (wrows[i]) {
			printf(" Model %d has %2d rows and %3d columns.\n",
			    i, wrows[i] - 1, wcols[i]);
		}
	}
	for (;;) {
		printf("\nEnter model number: (2, 3, 4 or 5) [4] ");
		fflush(stdout);
		if (get_input(inbuf, sizeof(inbuf)) == NULL) {
			return -1;
		}
		if (!inbuf[0]) {
			break;
		}
		u = strtoul(inbuf, &ptr, 10);
		if (u < 2 || u > 5 || *ptr != '\0') {
			printf("Invalid model number.\n");
			continue;
		}
		s->model = (int)u;
		break;
	}
	return 0;
}

int
get_charset(session_t *s)
{
    	int i;
	char *ptr;
	unsigned long u;

	new_screen(s, "\
Character Set\n\
\n\
This specifies the EBCDIC character set used by the host.");


	printf("\nAvailable character sets:\n");
	for (i = 0; charsets[i] != NULL; i++) {
		if (i && !(i % CS_COLS))
			printf("\n");
		printf(" %2d) %-*s", i + 1, CS_WIDTH - 5, charsets[i]);
	}
	printf("\n");
	for (;;) {
		printf("\nCharacter set: [bracket] ");
		if (get_input(s->charset, sizeof(s->charset)) == NULL) {
			return -1;
		}
		if (!s->charset[0]) {
			strcpy(s->charset, "bracket");
			break;
		}
		u = strtoul(s->charset, &ptr, 10);
		if (u > 0 && u <= i && *ptr == '\0') {
			strcpy(s->charset, charsets[u - 1]);
			break;
		}
		for (i = 0; charsets[i] != NULL; i++) {
			if (!strcmp(s->charset, charsets[i]))
				break;
		}
		if (charsets[i] != NULL)
			break;
		printf("Ivalid character set name.\n");
	}
	return 0;
}

#if defined(HAVE_LIBSSL) /*[*/
int
get_ssl(session_t *s)
{
    	new_screen(s, "\
SSL Tunnel\n\
\n\
This option causes wc3270 to first create a tunnel to the host using the\n\
Secure Sockets Layer (SSL), then to run the TN3270 session inside the tunnel.");

	do {
		printf("\nUse an SSL tunnel? (y/n) [n] ");
		fflush(stdout);
		s->ssl = getyn(0);
		if (s->ssl == -1)
			return -1;
	} while (s->ssl < 0);
	return 0;
}
#endif /*]*/

int
get_wpr3287(session_t *s)
{
    	new_screen(s, "\
wpr3287 Session\n\
\n\
This option allows wc3270 to automatically start a wpr3287 printer session\n\
when it connects to the host, allowing the host to direct print jobs to a\n\
Windows printer.");

	do {
		printf("\nAutomatically start a wpr3287 printer session? (y/n) [n] ");
		fflush(stdout);
		s->wpr3287 = getyn(0);
		if (s->wpr3287 == -1)
		    	return -1;
	} while (s->wpr3287 < 0);
	return 0;
}

int
get_printerlu(session_t *s)
{
	int rc;

	new_screen(s, "\
wpr3287 Session -- Printer Logical Unit (LU) Name\n\
\n\
The wpr3287 printer session can be configured in one of two ways.  The first\n\
method automatically associates the printer session with the current login\n\
session.  The second method specifies a particular Logical Unit (LU) to use\n\
for the printer session.");

	for (;;) {
		printf("\nAssociate the printer session with the current login session (y/n) [y]: ");
		fflush(stdout);
		rc = getyn(1);
		switch (rc) {
		case -1:
		    	return -1;
		case -2:
		default:
			continue;
		case 0:
			break;
		case 1:
			strcpy(s->printerlu, ".");
			break;
		}
		break;
	}

	while (!s->printerlu[0]) {
		printf("\nEnter printer Logical Unit (LU) name: ");
		fflush(stdout);
		if (get_input(s->printerlu, sizeof(s->printerlu)) == NULL)
			return -1;
	}

	return 0;
}

int
get_printer(session_t *s)
{
    	int i;
	char *ptr;
	unsigned long u;

	new_screen(s, "\
wpr3287 Session -- Windows Printer Name\n\
\n\
The wpr3287 session can use the Windows default printer as its real printer,\n\
or you can specify a particular Windows printer.  You can specify a local\n\
printer, or specify a remote printer with a UNC path, e.g.,\n\
'\\\\server\\printer22'.");

	enum_printers();
	if (num_printers) {
		printf("\nWindows printers (default is '*'):\n");
		for (i = 0; i < num_printers; i++) {
			printf(" %2d) %c %s\n", i + 1,
				strcasecmp(default_printer,
				    printer_info[i].pName)? ' ': '*',
				printer_info[i].pName);
		}
		printf(" %2d)   Other\n", num_printers + 1);
		for (;;) {
			printf("\nEnter Windows printer (1-%d): [use system default] ",
				num_printers + 1);
			fflush(stdout);
			if (get_input(s->printer, sizeof(s->printer))
				    == NULL)
				return -1;
			if (!s->printer[0])
				break;
			u = strtoul(s->printer, &ptr, 10);
			if (*ptr != '\0' || u == 0 ||
				    u > num_printers + 1)
				continue;
			if (u == num_printers + 1) {
				if (get_printer_name(s->printer) < 0)
					return -1;
				break;
			}
			strcpy(s->printer, printer_info[u - 1].pName);
			break;
		}
	} else {
		if (get_printer_name(s->printer) < 0)
			return -1;
	}
	return 0;
}

int
get_keymaps(session_t *s)
{
    	int i;
	HANDLE h;
	WIN32_FIND_DATA find_data;
	km_t *km;

	new_screen(s, "\
Keymaps\n\
\n\
A keymap is a mapping from the PC keyboard to the virtual 3270 keyboard.\n\
You can override the default keymap and specify one or more built-in or \n\
user-defined keymaps (if any), separated by commas.");

	printf("\nBuilt-in keymaps:\n");
	for (i = 0; builtin_keymaps[i].name != NULL; i++) {
	    	printf(" %2d) %s\n    %s\n",
			i + 1,
			builtin_keymaps[i].name,
			builtin_keymaps[i].description);
		(void) save_keymap_name(builtin_keymaps[i].name);
	}
	h = FindFirstFile(".\\*.wc3270km", &find_data);
	if (h != INVALID_HANDLE_VALUE) {
		do {
		    	char *n;

			n = save_keymap_name(find_data.cFileName);
			if (n != NULL) {
				printf(" %2d) %s\n    User-defined\n",
					++i + 1, n);
			}
		} while (FindNextFile(h, &find_data) != 0);
		FindClose(h);
	}
	for (;;) {
	    	char inbuf[STR_SIZE];
	    	char tknbuf[STR_SIZE];
		char *t;
		char *buf;
		int wrong = FALSE;

	    	printf("\nEnter keymap(s) [none]: ");
		fflush(stdout);
		if (get_input(inbuf, sizeof(inbuf)) == NULL)
			return -1;
		if (!inbuf[0])
		    	break;
		strcpy(tknbuf, inbuf);
		wrong = FALSE;
		buf = tknbuf;
		while (!wrong && (t = strtok(buf, ",")) != NULL) {
		    	buf = NULL;
			for (km = km_first; km != NULL; km = km->next) {
				if (!strcasecmp(t, km->name))
					break;
			}
			if (km == NULL) {
			    	printf("\nInvalid keymap name '%s'\n", t);
				wrong = TRUE;
				break;
			}
		}
		if (!wrong) {
			strcpy(s->keymaps, inbuf);
			break;
		}
	}
	return 0;
}

int
summarize_and_proceed(session_t *s)
{
    	int rc;

	new_screen(s, "");

	printf("                      Host: %s\n", s->host);
	if (s->luname[0])
	    printf("         Logical Unit Name: %s\n", s->luname);
	printf("                  TCP Port: %d\n", s->port);
	printf("              Model Number: %d (%d rows x %d columns)\n",
	    s->model, wrows[s->model] - 1, wcols[s->model]);
	printf("             Character Set: %s\n", s->charset);
#if defined(HAVE_LIBSSL) /*[*/
	printf("                SSL Tunnel: %s\n", s->ssl? "Yes": "No");
#endif /*]*/
	printf("   wpr3287 Printer Session: %s\n", s->wpr3287? "Yes": "No");
	if (s->wpr3287) {
	        printf("              wpr3287 Mode: ");
		if (!strcmp(s->printerlu, "."))
			printf("Associate\n");
		else
		    	printf("LU %s\n", s->printerlu);
		printf("   wpr3287 Windows printer: %s\n",
			s->printer[0]? s->printer: "(system default)");
	}
	if (s->keymaps[0])
	    	printf("                   Keymaps: %s\n", s->keymaps);

	for (;;) {
		printf("\nCreate the session? (y/n) [y] ");
		fflush(stdout);
		rc = getyn(1);
		if (rc == -1 || rc == 0)
			return -1;
		if (rc == 1)
			break;
	}
	return 0;
}

int
session_wizard(void)
{
    	session_t session;
	int rc;
	char *userprof;
	char desktop[MAX_PATH];
	char installdir[MAX_PATH];
	char path[1024];
	char linkpath[MAX_PATH];
	char exepath[MAX_PATH];
	HRESULT hres;

	/* Start with nothing. */
	(void) memset(&session, '\0', sizeof(session));

	/* Figure out where the install directory is. */
	if (getcwd(installdir, MAX_PATH) == NULL) {
		printf("getcwd failed: %s\n", strerror(errno));
		return -1;
	}

	/* Figure out where the desktop is. */
	userprof = getenv("USERPROFILE");
	if (userprof == NULL) {
		printf("Sorry, I can't figure out where your user profile "
			"is.\n");
		return -1;
	}
	sprintf(desktop, "%s\\Desktop", userprof);

	/* Intro screen. */
	if (intro(&session) < 0)
		return -1;

	/* Get the session name. */
	rc = get_session(&session, installdir);
	if (rc == -1)
	    	return -1;

	if (rc == 0) {

	    /* Get the host name, which defaults to the session name. */
	    if (get_host(&session) < 0)
		    return -1;

	    /* Get the port. */
	    if (get_port(&session) < 0)
		    return -1;

	    /* Get the LU name. */
	    if (get_lu(&session) < 0)
		    return -1;

	    /* Get the model number. */
	    if (get_model(&session) < 0)
		    return -1;

	    /* Get the character set name. */
	    if (get_charset(&session) < 0)
		    return -1;

#if defined(HAVE_LIBSSL) /*[*/
	    /* Get the SSL tunnel information. */
	    if (get_ssl(&session) < 0)
		    return -1;
#endif /*]*/

	    /* Ask about a wpr3287 session. */
	    if (get_wpr3287(&session) < 0)
		    return -1;

	    if (session.wpr3287) {
		    if (get_printerlu(&session) < 0)
			    return -1;
		    if (get_printer(&session) < 0)
			    return -1;
	    }

	    /* Ask about keymaps. */
	    if (get_keymaps(&session) < 0)
		    return -1;

	    /* Summarize and make sure they want to proceed. */
	    if (summarize_and_proceed(&session) < 0)
		    return -1;

	    /* Create the session file. */
	    printf("\nCreating session file... ");
	    fflush(stdout);
	    if (create_session_file(&session) < 0)
		    return -1;
	    printf("done\n");
	    fflush(stdout);
	}

	/* Ask about the shortcut. */
	for (;;) {
	    	printf("\nCreate desktop shortcut (y/n) [y]: ");
		rc = getyn(1);
		if (rc <= 0)
		    	return -1;
		if (rc == 1)
		    	break;
	}

	/* Create the desktop shorcut. */
	printf("\nCreating desktop shortcut... ");
	fflush(stdout);
	sprintf(linkpath, "%s\\%s.lnk", desktop, session.session);
	sprintf(path, "\"%s.wc3270\"", session.session);
	sprintf(exepath, "%s\\wc3270.exe", installdir);
	hres = CreateLink(
		exepath,			/* path to executable */
		linkpath,			/* where to put the link */
		"wc3270 session",		/* description */
		path,				/* arguments */
		installdir,			/* working directory */
		wrows[session.model], wcols[session.model]);
						/* console rows, columns */
	if (SUCCEEDED(hres)) {
		printf("done\n");
		fflush(stdout);
		return 0;
	} else {
		printf("Failed\n");
		fflush(stdout);
		return -1;
	}
}

/* Create the session file. */
int
create_session_file(session_t *session)
{
    	FILE *f;
	time_t t;
	int bracket;

	f = fopen(session->path, "w");
	if (f == NULL) {
		perror("Cannot create session file");
		return -1;
	}

	fprintf(f, "! wc3270 session '%s'\n", session->session);

	t = time(NULL);
	fprintf(f, "! Created by the wc3270 %s session wizard %s",
		wversion, ctime(&t));

	bracket = (strchr(session->host, ':') != NULL);
	fprintf(f, "wc3270.hostname: ");
	if (session->ssl)
	    	fprintf(f, "L:");
	if (session->luname[0])
	    	fprintf(f, "%s@", session->luname);
	fprintf(f, "%s%s%s",
		bracket? "[": "",
		session->host,
		bracket? "]": "");
	if (session->port != 23)
	    	fprintf(f, ":%d", session->port);
	fprintf(f, "\n");

	fprintf(f, "wc3270.model: %d\n", session->model);
	fprintf(f, "wc3270.charset: %s\n", session->charset);

	if (session->wpr3287) {
	    	fprintf(f, "wc3270.printerLu: %s\n", session->printerlu);
		if (session->printer[0])
		    	fprintf(f, "wc3270.printer.name: %s\n",
				session->printer);
	}

	if (session->keymaps[0]) {
	    	fprintf(f, "wc3270.keymap: %s\n", session->keymaps);
	}

	fclose(f);

	return 0;
}

int
main(int argc, char *argv[])
{
	int rc;
	char buf[2];

	system("cls");
	rc = session_wizard();

	printf("\nWizard %s.  [Press <Enter>] ",
		    (rc < 0)? "aborted": "complete");
	fflush(stdout);
	(void) fgets(buf, 2, stdin);

	return 0;
}
