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
 *	mkshort.c
 *		A Windows console-based 3270 Terminal Emulator
 *		Quick standalone utility to Create a shortcut to
 *		wc3270.exe on the desktop with the right properties.
 */

#include "globals.h"

#include <windows.h>
#include <wincon.h>
#include <shlobj.h>
#include "shlobj_missing.h" /* missing IShellLinkDataist stuff from MinGW */

#define INSTALL_DIR	"C:\\Program Files\\wc3270"

/*
 * CreateLink - uses the shell's IShellLink, IShellLinkDataList and
 * IPersistFile interfaces to create and store a shortcut to the specified
 *  object. 
 * Returns the result of calling the member functions of the interfaces. 
 * lpszPathObj - address of a buffer containing the path of the object 
 * lpszPathLink - address of a buffer containing the path where the 
 *  shell link is to be stored 
 * lpszDesc - address of a buffer containing the description of the 
 *   shell link 
 * lpszArgs - address of a buffer containing the arguments to the command
 * rows - number of rows for the console
 * cols - number of cols for the console
 */
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

	/* Get a pointer to the IShellLink interface. */
	hres = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
	    &IID_IShellLink, (LPVOID *)&psl);

	if (!SUCCEEDED(hres))
		goto out;

	/*
	 * Set the path to the shortcut target, and add the description and
	 * arguments.
	 */
	psl->lpVtbl->SetPath(psl, lpszPathObj);
	if (lpszDesc)
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
	p.dwFontSize.Y = 14; /* ? */
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

	/*
	 * Query IShellLink for the IPersistFile interface for saving
	 * the shortcut in persistent storage.
	 */
	hres = psl->lpVtbl->QueryInterface(psl, &IID_IPersistFile,
	    (void **)&ppf);

	if (!SUCCEEDED(hres))
		goto out;

	/* Convert the path to Unicode. */
	MultiByteToWideChar(CP_ACP, 0, lpszPathLink, -1, wsz, MAX_PATH);

	/* Save the link by calling IPersistFile::Save. */
	hres = ppf->lpVtbl->Save(ppf, wsz, TRUE);

out:
	/* Clean up. */
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
main(int argc, char *argv[])
{
	char *userprof;
	char linkpath[MAX_PATH];
	HRESULT hres;

	/* Figure out the link path. */
	userprof = getenv("USERPROFILE");
	if (userprof == NULL) {
		fprintf(stderr, "Sorry, I can't figure out where your user "
			"profile is.\n");
		return 1;
	}
	sprintf(linkpath, "%s\\Desktop\\wc3270.lnk", userprof);

	/* Create the link. */
	hres = CreateLink(
		INSTALL_DIR "\\wc3270.exe",
		linkpath,
		NULL,
		NULL,
		INSTALL_DIR,
		44,
		80);
	if (hres) {
		fprintf(stderr, "Link creation failed.\n");
	}

	return hres;
}
