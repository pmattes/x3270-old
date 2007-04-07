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
#define CS_WIDTH	16
#define	CS_COLS		(79 / CS_WIDTH)

             /*  model  2   3   4   5 */
int wrows[6] = { 0, 0, 25, 33, 44, 28  };
int wcols[6] = { 0, 0, 80, 80, 80, 132 };

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

int
getyn(int defval)
{
	char yn[STR_SIZE];
	int sl;

	if (fgets(yn, sizeof(yn), stdin) == NULL) {
		return -1;
	}

	sl = strlen(yn);
	if (sl > 0 && yn[sl - 1] == '\n')
		yn[--sl] = '\0';
	if (!sl)
		return defval;

	if (!strncasecmp(yn, "yes", sl))
		return 1;
	if (!strncasecmp(yn, "no", sl))
		return 0;

	printf("Please answer (y)es or (n)o.\n\n");
	return -2;
}

int
session_wizard(void)
{
	char *userprof;
	char desktop[MAX_PATH];
	char *programfiles;
	char installdir[MAX_PATH];
	char cname[STR_SIZE];
	char hname[STR_SIZE];
	char port[STR_SIZE];
	char luname[STR_SIZE];
	int ssl = 0;
	unsigned long model = 4;
	int sl;
	char path[1024];
	FILE *f;
	time_t t;
	char linkpath[MAX_PATH];
	char exepath[MAX_PATH];
	HRESULT hres;
	int rc;
	int i;
	int bracket = 0;
	char charset[STR_SIZE];

	printf(
"wc3270 Session Wizard                                            v3.3.5p7\n\n\
This wizard sets up a new wc3270 session.\n\n\
It creates a session profile in the wc3270 installation directory and a\n\
shortcut on your desktop.\n\n");

	for (;;) {
		int	rc;

		printf("Continue? (y/n) [y] ");
		fflush(stdout);
		rc = getyn(1);
		if (rc == -1 || rc == 0)
			return -1;
		if (rc == 1)
			break;
	}

	/* Figure out where the install directory is. */
	programfiles = getenv("PROGRAMFILES");
	if (programfiles == NULL)
		programfiles = "C:\\Program Files";
	sprintf(installdir, "%s\\wc3270", programfiles);

	/* Figure out where the desktop is. */
	userprof = getenv("USERPROFILE");
	if (userprof == NULL) {
		printf("Sorry, I can't figure out where your user profile "
			"is.\n");
		return -1;
	}
	sprintf(desktop, "%s\\Desktop", userprof);

	/* Get the session name. */
	for (;;) {
		printf("\nSession name: ");
		fflush(stdout);
		if (fgets(cname, sizeof(cname), stdin) == NULL) {
			return -1;
		}
		sl = strlen(cname);
		if (sl > 0 && cname[sl - 1] == '\n')
			cname[--sl] = '\0';
		if (!sl)
			continue;

		if (strspn(cname, LEGAL_CNAME) != sl) {
			fprintf(stdout, "\
\nIllegal character(s).\n\
Session names can only have letters, numbers, spaces, underscore '_'\n\
and dash '-')\n");
			continue;
		}

		break;
	}
	sprintf(path, "%s\\%s.wc3270", installdir, cname);
	f = fopen(path, "r");
	if (f != NULL) {
		for (;;) {
			printf("\nSession '%s' already exists.  "
			    "Overwrite it? (y/n) [y] ", cname);
			fflush(stdout);
			rc = getyn(1);
			if (rc == -1 || rc == 0)
				return -1;
			if (rc == 1)
				break;
		}
		fclose(f);
	}

	/* Get the host name, which defaults to the session name. */
	for (;;) {
		if (strchr(cname, ' ') == NULL)
			printf("\nHostname or IP address: [%s] ", cname);
		else
			printf("\nHostname or IP address: ");
		fflush(stdout);
		if (fgets(hname, sizeof(hname), stdin) == NULL) {
			return -1;
		}
		sl = strlen(hname);
		if (sl > 0 && hname[sl - 1] == '\n')
			hname[--sl] = '\0';
		if (strchr(hname, ' ') != NULL) {
			printf("\nHostnames cannot contain spaces.\n");
			continue;
		}
		if (strchr(hname, '@') != NULL) {
			printf("\nHostnames cannot contain '@' characters.\n");
			continue;
		}
		if (strchr(hname, ':') != NULL) {
			if (hname[0] != '[' &&
			    hname[strlen(hname) - 1] != ']') {

				if (strchr(hname, '[') != NULL ||
				    strchr(hname, ']') != NULL) {

					printf("\nHostnames cannot contain "
					    "both ':' characters and '[' or "
					    "']' characters.\n");
					continue;
				}

				bracket = 1;
			}
		}
		if (!sl) {
			if (strchr(cname, ' ') != NULL)
				continue;
			strcpy(hname, cname);
		}
		break;
	}

	/* Get the port. */
	for (;;) {
		unsigned long pn;
		char *ptr;

		printf("\nTCP port: [telnet] ");
		if (fgets(port, sizeof(port), stdin) == NULL) {
			return -1;
		}
		sl = strlen(port);
		if (sl > 0 && port[sl - 1] == '\n')
			port[--sl] = '\0';
		if (!sl)
			break;
		if (!strcasecmp(port, "telnet")) {
			port[0] = '\0';
			break;
		}
		pn = strtoul(port, &ptr, 10);
		if (pn < 1 || pn > 65535 || *ptr != '\0') {
			printf("Invalid port.  Port must be between 1 and 65535 or the string 'telnet'\n");
		} else {
			break;
		}
	}

	/* Get the LU name. */
	printf("\nLogical Unit (LU) name: [none] ");
	fflush(stdout);
	if (fgets(luname, sizeof(luname), stdin) == NULL) {
		return -1;
	}
	sl = strlen(luname);
	if (sl > 0 && luname[sl - 1] == '\n')
		luname[--sl] = '\0';

	/* Get the model number. */
	printf("\n");
	for (i = 2; i <= 5; i++) {
		if (wrows[i]) {
			printf("Model %d has %2d rows and %3d columns.\n",
			    i, wrows[i] - 1, wcols[i]);
		}
	}
	for (;;) {
		char nbuf[STR_SIZE];
		char *ptr;

		printf("\nModel number: (2, 3, 4 or 5) [4] ");
		fflush(stdout);
		if (fgets(nbuf, sizeof(nbuf), stdin) == NULL) {
			return -1;
		}
		sl = strlen(nbuf);
		if (sl > 0 && nbuf[sl - 1] == '\n')
			nbuf[--sl] = '\0';
		if (!sl) {
			model = 4;
			break;
		}
		model = strtoul(nbuf, &ptr, 10);
		if (model < 2 || model > 5 || *ptr != '\0') {
			printf("Invalid model number.\n");
			continue;
		}
		break;
	}

	/* Get the character set name. */
	printf("\nAvailable Character Sets:\n");
	for (i = 0; charsets[i] != NULL; i++) {
		if (i && !(i % CS_COLS))
			printf("\n");
		printf("%*s", CS_WIDTH, charsets[i]);
	}
	printf("\n");
	for (;;) {
		printf("\nCharacter set: [bracket] ");
		if (fgets(charset, sizeof(charset), stdin) == NULL) {
			return -1;
		}
		sl = strlen(charset);
		if (sl > 0 && charset[sl - 1] == '\n')
			charset[--sl] = '\0';
		if (!sl) {
			strcpy(charset, "bracket");
			break;
		}
		for (i = 0; charsets[i] != NULL; i++) {
			if (!strcmp(charset, charsets[i]))
				break;
		}
		if (charsets[i] != NULL)
			break;
		printf("Invalid character set name.\n");
	}

#if defined(HAVE_LIBSSL) /*[*/
	/* Get the SSL tunnel information. */
	do {
		printf("\nUse an SSL tunnel? (y/n) [n] ");
		fflush(stdout);
		ssl = getyn(0);
		if (ssl == -1)
			return -1;
	} while (ssl < 0);
#endif /*]*/

	system("cls");
	printf("Session Summary\n\n");
	printf(" Session Name:              %s\n", cname);
	printf(" Host Name or IP address:   %s\n", hname);
	if (luname[0])
	    printf(" Logical Unit Name:         %s\n", luname);
	printf(" Port:                      %s\n",
	    port[0]? port: "telnet (23)");
	printf(" Model Number:              %ld (%d rows x %d columns)\n",
	    model, wrows[model] - 1, wcols[model]);
	printf(" Character Set:             %s\n", charset);
#if defined(HAVE_LIBSSL) /*[*/
	printf(" SSL Tunnel:                %s\n", ssl? "Yes": "No");
#endif /*]*/
	for (;;) {
		printf("\nProceed? (y/n) [y] ");
		fflush(stdout);
		rc = getyn(1);
		if (rc == -1 || rc == 0)
			return -1;
		if (rc == 1)
			break;
	}

	/* Create the profile file. */
	printf("\nCreating profile... ");
	fflush(stdout);
	f = fopen(path, "w");
	if (f == NULL) {
		perror("Cannot create profile");
		return -1;
	}
	fprintf(f, "! wc3270 profile '%s'\n", cname);
	t = time(NULL);
	fprintf(f, "! Created by the wc3270 session wizard %s", ctime(&t));
	fprintf(f, "wc3270.hostname: %s%s%s%s%s%s%s%s\n",
	    ssl? "L:": "",
	    luname[0]? luname: "",
	    luname[0]? "@": "",
	    bracket? "[": "", hname, bracket? "]": "",
	    port[0]? ":": "",
	    port[0]? port: "");
	fprintf(f, "wc3270.model: %ld\n", model);
	fprintf(f, "wc3270.charset: %s\n", charset);
	fclose(f);
	printf("done\n");
	fflush(stdout);

	/* Create the desktop shorcut. */
	printf("\nCreating desktop link... ");
	fflush(stdout);
	sprintf(linkpath, "%s\\%s.lnk", desktop, cname);
	sprintf(path, "\"%s.wc3270\"", cname);
	sprintf(exepath, "%s\\wc3270.exe", installdir);
	hres = CreateLink(
		exepath,			/* path to executable */
		linkpath,			/* where to put the link */
		"wc3270 session",		/* description */
		path,				/* arguments */
		installdir,			/* working directory */
		wrows[model], wcols[model]);	/* console rows, columns */
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
