/*
 * Copyright 2000, 2001 by Paul Mattes.
 *   Permission to use, copy, modify, and distribute this software and its
 *   documentation for any purpose and without fee is hereby granted,
 *   provided that the above copyright notice appear in all copies and that
 *   both that copyright notice and this permission notice appear in
 *   supporting documentation.
 */

/*
 *	Help.c
 *		Help information for c3270.
 */

#include "globals.h"
#include "appres.h"
#include "resources.h"

#include "actionsc.h"
#include "popupsc.h"
#include "screenc.h"

#define P_3270		0x0001	/* 3270 actions */
#define P_SCRIPTING	0x0002	/* scripting actions */
#define P_INTERACTIVE	0x0004	/* interactive (command-prompt) actions */
#define P_OPTIONS	0x0008	/* command-line options */
#define P_TRANSFER	0x0010	/* file transfer options */

static struct {
	const char *name;
	const char *args;
	int purpose;
	const char *help;
} cmd_help[] = {
	{ "Abort",	CN, P_SCRIPTING, "Abort pending scripts and macros" },
	{ "AnsiText",	CN, P_SCRIPTING, "Dump pending NVT text" },
	{ "Ascii",	CN, P_SCRIPTING, "Screen contents in ASCII" },
	{ "Ascii",	"<n>", P_SCRIPTING, "<n> bytes of screen contents from cursor, in ASCII" },
	{ "Ascii",	"<row> <col> <n>", P_SCRIPTING, "<n> bytes of screen contents from <row>,<col>, in ASCII" },
	{ "Ascii",	"<row> <col> <rows> <cols>", P_SCRIPTING, "<rows>x<cols> of screen contents from <row>,<col>, in ASCII" },
	{ "AsciiField", CN, P_SCRIPTING, "Contents of current field, in ASCII" },
	{ "Attn", CN, P_3270, "Sent 3270 ATTN sequence (TELNET IP)" },
	{ "BackSpace", CN, P_3270, "Move cursor left" },
	{ "BackTab", CN, P_3270, "Move to previous field" },
	{ "CircumNot", CN, P_3270, "Send ~ in NVT mode, \254 in 3270 mode" },
	{ "Clear", CN, P_3270, "Send CLEAR AID (clear screen)" },
	{ "Close", CN, P_INTERACTIVE, "Alias for 'Disconnect'" },
	{ "CloseScript", CN, P_SCRIPTING, "Exit peer script" },
	{ "Connect", "[<lu>@]<host>[:<port>]", P_INTERACTIVE, "Open connection to <host>" },
#if defined(LOCAL_PROCESS) /*[*/
	{ "Connect", "-e [<command> [<arg>...]]", P_INTERACTIVE, "Open connection to a local shell or command" },
#endif /*]*/
	{ "ContinueScript", CN, P_SCRIPTING, "Resume paused script" },
	{ "CursorSelect", CN, P_3270, "Light pen select at cursor location" },
	{ "Delete", CN, P_3270, "Delete character at cursor" },
	{ "DeleteField", CN, P_3270, "Erase field at cursor location (^U)" },
	{ "DeleteWord", CN, P_3270, "Erase word before cursor location (^W)" },
	{ "Disconnect", CN, P_INTERACTIVE, "Close connection to host" },
	{ "Down", CN, P_3270, "Move cursor down" },
	{ "Dup", CN, P_3270, "3270 DUP key (X'1C')" },
	{ "Ebcdic",	CN, P_SCRIPTING, "Screen contents in EBCDIC" },
	{ "Ebcdic",	"<n>", P_SCRIPTING, "<n> bytes of screen contents from cursor, in EBCDIC" },
	{ "Ebcdic",	"<row> <col> <n>", P_SCRIPTING, "<n> bytes of screen contents from <row>,<col>, in EBCDIC" },
	{ "Ebcdic",	"<row> <col> <rows> <cols>", P_SCRIPTING, "<rows>x<cols> of screen contents from <row>,<col>, in EBCDIC" },
	{ "EbcdicField", CN, P_SCRIPTING, "Contents of current field, in EBCDIC" },
	{ "Enter", CN, P_3270, "Send ENTER AID" },
	{ "Erase", CN, P_3270, "Destructive backspace" },
	{ "EraseEOF", CN, P_3270, "Erase from cursor to end of field" },
	{ "EraseInput", CN, P_3270, "Erase all input fields" },
	{ "Execute", "<command>", P_SCRIPTING, "Execute a shell command" },
	{ "Expect", "<pattern>", P_SCRIPTING, "Wait for NVT output" },
	{ "FieldEnd", CN, P_3270, "Move to end of field" },
	{ "FieldExit", CN, P_3270, "Erase to end of field, move to next (5250)" },
	{ "FieldMark", CN, P_3270, "3270 FIELD MARK key (X'1E')" },
	{ "Flip", CN, P_3270, "Flip display left-to-right" },
	{ "Help", "all|interactive|3270|scripting|transfer<cmd>", P_INTERACTIVE, "Get help" },
	{ "HexString", "<digits>", P_3270|P_SCRIPTING, "Input field data in hex" },
	{ "Home", CN, P_3270, "Move cursor to first field" },
	{ "ignore", CN, P_3270, "Do nothing" },
	{ "Insert", CN, P_3270, "Set 3270 insert mode" },
	{ "Key", "<symbol>|0x<nn>", P_3270, "Input one character" },
	{ "Left", CN, P_3270, "Move cursr left" },
	{ "Left2", CN, P_3270, "Move cursor left 2 columns" },
	{ "MonoCase", CN, P_3270, "Toggle monocase mode" },
	{ "MoveCursor", "<row> <col>", P_3270|P_SCRIPTING, "Move cursor to specific location" },
	{ "Newline", CN, P_3270, "Move cursor to first field in next row" },
	{ "NextWord", CN, P_3270, "Move cursor to next word" },
	{ "Open", CN, P_INTERACTIVE, "Alias for 'Connect'" },
	{ "PA", "<n>", P_3270, "Send 3270 Program Attention" },
	{ "PauseScript", CN, P_SCRIPTING, "Pause script until ResumeScript" },
	{ "PF", "<n>", P_3270, "Send 3270 PF AID" },
	{ "PreviousWord", CN, P_3270, "Move cursor to previous word" },
	{ "Printer", "Start[,lu]|Stop", P_3270|P_SCRIPTING|P_INTERACTIVE, "Start or stop pr3287 printer session" },
	{ "Quit", CN, P_INTERACTIVE, "Exit c3270" },
	{ "Redraw", CN, P_INTERACTIVE|P_3270, "Redraw screen" },
	{ "Reset", CN, P_3270, "Clear keyboard lock" },
	{ "Right", CN, P_3270, "Move cursor right" },
	{ "Right2", CN, P_3270, "Move cursor right 2 columns" },
	{ "Script", "<path> [<arg>...]", P_SCRIPTING, "Run a child script" },
	{ "Show", CN, P_INTERACTIVE, "Display status and settings" },
	{ "Snap", "<args>", P_SCRIPTING, "Screen snapshot manipulation" },
	{ "String", "<text>", P_3270|P_SCRIPTING, "Input a string" },
	{ "SysReq", CN, P_3270, "Send 3270 Attention (TELNET ABORT or SYSREQ AID)" },
	{ "Tab", CN, P_3270, "Move cursor to next field" },
	{ "ToggleInsert", CN, P_3270, "Set or clear 3270 insert mode" },
	{ "ToggleReverse", CN, P_3270, "Set or clear reverse-input mode" },
	{ "Trace", "[data|keyboard]on|off [<file>]", P_INTERACTIVE, "Configure tracing" },
	{ "Transfer", "<args>", P_INTERACTIVE, "IND$FILE file transfer" },
	{ "Up", CN, P_3270, "Move cursor up" },
	{ "Wait", "<args>", P_SCRIPTING, "Wait for host events" },
	{ CN,  CN, 0, CN }
};

static struct {
	const char *name;
	int flag;
	const char *text;
} help_subcommand[] = {
	{ "all",		-1,		CN },
	{ "3270",		P_3270,		CN },
	{ "interactive",	P_INTERACTIVE,	CN },
	{ "options",		P_OPTIONS,
"Command-line options:\n"
"  " OptCharset " <name>\n"
"    Use EBCDIC character set <name>\n"
"  " OptClear " <toggle>\n"
"    Turn off the specified <toggle> option\n"
"  " OptDsTrace "\n"
"    Turn on data stream tracing\n"
"  " OptHostsFile " <file>\n"
"    Use <file> as the hosts file\n"
"  " OptKeymap " <file>\n"
"    Use the keymap in <file>\n"
"  " OptModel " <n>\n"
"    Emulate a 327x model <n>\n"
"  " OptMono "\n"
"    Emulate a monochrome 3278, even if the terminal can display color\n"
"  " OptOversize " <cols>x<rows>\n"
"    Make the screen oversized to <cols>x<rows>\n"
"  " OptSet " <toggle>\n"
"    Turn on the specified <toggle> option\n"
"  " OptTermName " <name>\n"
"    Send <name> as the TELNET terminal name\n"
"  -xrm \"c3270.<resourcename>: <value>\"\n"
"    Set a resource value" },
	{ "scripting",		P_SCRIPTING,	CN },
	{ "file-transfer",	P_TRANSFER,
"Syntax:\n"
"  Transfer <keyword>=<value>...\n"
"Keywords:\n"
"  Direction=send|receive               default 'send'\n"
"  HostFile=<path>                      required\n"
"  LocalFile=<path>                     required\n"
"  Host=tso|vm                          default 'tso'\n"
"  Mode=ascii|binary                    default 'ascii'\n"
"  Cr=remove|add|keep                   default 'remove'\n"
"  Exist=keep|replace|append            default 'keep'\n"
"  Recfm=fixed|variable|undefined       for Direction=send\n"
"  Lrecl=<n>                            for Direction=send\n"
"  Blksize=<n>                          for Direction=send Host=tso\n"
"  Allocation=tracks|cylinders|avblock  for Direction=send Host=tso\n"
"  PrimarySpace=<n>                     for Direction=send Host=tso\n"
"  SecondarySpace=<n>                   for Direction=send Host=tso\n"
"Note that to embed a space in a value, you must quote the keyword, e.g.:\n"
"  Transfer Direction=send LocalFile=/tmp/foo \"HostFile=foo text a\" Host=vm"
	    },
	{ CN, 0, CN }
};

/* c3270-specific actions. */
void
Help_action(Widget w unused, XEvent *event unused, String *params,
    Cardinal *num_params)
{
	int i;
	int overall = -1;
	int match = 0;

	action_debug(Help_action, event, params, num_params);

	if (*num_params != 1) {
		action_output(
"  help all           all commands\n"
"  help 3270          3270 commands\n"
"  help interactive   interactive (command-prompt) commands\n"
"  help <command>     help for one <command>\n"
"  help options       command-line options\n"
"  help scripting     scripting commands\n"
"  help file-transfer file transfer options");
		return;
	}

	for (i = 0; help_subcommand[i].name != CN; i++) {
		if (!strncasecmp(help_subcommand[i].name, params[0],
		    strlen(params[0]))) {
			match = help_subcommand[i].flag;
			overall = i;
			break;
		}
	}
	if (match) {
		for (i = 0; cmd_help[i].name != CN; i++) {
			if (!strncasecmp(cmd_help[i].name, params[0],
			    strlen(params[0]))) {
				action_output("Ambiguous: matches '%s' and "
				    "one or more commands",
				    help_subcommand[overall].name);
				return;
			}
		}
		if (help_subcommand[overall].text != CN) {
			action_output(help_subcommand[overall].text);
			return;
		}
		for (i = 0; cmd_help[i].name != CN; i++) {
			if (cmd_help[i].purpose & match) {
				action_output("  %s %s\n    %s",
				    cmd_help[i].name,
				    cmd_help[i].args? cmd_help[i].args: "",
				    cmd_help[i].help? cmd_help[i].help: "");
			}
		}
	} else {
		Boolean any = False;

		for (i = 0; cmd_help[i].name != CN; i++) {
			if (!strncasecmp(cmd_help[i].name, params[0],
			    strlen(params[0]))) {
				action_output("  %s %s\n    %s",
				    cmd_help[i].name,
				    cmd_help[i].args? cmd_help[i].args: "",
				    cmd_help[i].help? cmd_help[i].help: "");
				any = True;
			}
		}
		if (!any)
			action_output("No such command: %s", params[0]);
	}
}
