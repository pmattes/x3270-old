/*
 * Copyright 1995, 1996, 1999, 2000 by Paul Mattes.
 *  Permission to use, copy, modify, and distribute this software and its
 *  documentation for any purpose and without fee is hereby granted,
 *  provided that the above copyright notice appear in all copies and that
 *  both that copyright notice and this permission notice appear in
 *  supporting documentation.
 */

/*
 *	resources.h
 *		x3270 resource and option names.
 */

/* Resources. */
#define ResActiveIcon		"activeIcon"
#define ResAdVersion		"adVersion"
#define ResAllowResize		"allowResize"
#define ResAltCursor		"altCursor"
#define ResAplFont		"aplFont"
#define ResAplMode		"aplMode"
#define ResAssocCommand		"printer.assocCommandLine"
#define ResBaselevelTranslations	"baselevelTranslations"
#define ResBellVolume		"bellVolume"
#define ResBlankFill		"blankFill"
#define ResBoldColor		"boldColor"
#define ResCharClass		"charClass"
#define ResCharset		"charset"
#define ResCharsetList		"charsetList"
#define ResCodepage		"codepage"
#define ResColorBackground	"colorBackground"
#define ResColorScheme		"colorScheme"
#define ResComposeMap		"composeMap"
#define ResConnectFileName	"connectFileName"
#define ResCursorBlink		"cursorBlink"
#define ResCursorColor		"cursorColor"
#define ResCursorPos		"cursorPos"
#define ResDebugFont		"debugFont"
#define ResDebugTracing		"debugTracing"
#define ResDisconnectClear	"disconnectClear"
#define ResDoConfirms		"doConfirms"
#define ResDsTrace		"dsTrace"
#define ResEmulatorFont		"emulatorFont"
#define ResEof			"eof"
#define ResErase		"erase"
#define ResEventTrace		"eventTrace"
#define ResExtended		"extended"
#define ResFontList		"fontMenuList"
#define ResFtCommand		"ftCommand"
#define ResHighlightBold	"highlightBold"
#define ResHighlightSelect	"highlightSelect"
#define ResHostsFile		"hostsFile"
#define ResIconFont		"iconFont"
#define ResIconLabelFont	"iconLabelFont"
#define ResIcrnl		"icrnl"
#define ResInlcr		"inlcr"
#define ResInputColor		"inputColor"
#define ResIntr			"intr"
#define ResInvertKeypadShift	"invertKeypadShift"
#define ResKeymap		"keymap"
#define ResKeypad		"keypad"
#define ResKeypadBackground	"keypadBackground"
#define ResKeypadOn		"keypadOn"
#define ResKill			"kill"
#define ResLabelIcon		"labelIcon"
#define ResLineWrap		"lineWrap"
#define ResLnext		"lnext"
#define ResLockedCursor		"lockedCursor"
#define ResLuCommandLine	"printer.luCommandLine"
#define ResM3279		"m3279"
#define ResMacros		"macros"
#define ResMarginedPaste	"marginedPaste"
#define ResMenuBar		"menuBar"
#define ResModel		"model"
#define ResModifiedSel		"modifiedSel"
#define ResModifiedSelColor	"modifiedSelColor"
#define ResMono			"mono"
#define ResMonoCase		"monoCase"
#define ResNoOther		"noOther"
#define ResNormalColor		"normalColor"
#define ResNormalCursor		"normalCursor"
#define ResNumericLock		"numericLock"
#define ResOerrLock		"oerrLock"
#define ResOnce			"once"
#define ResOversize		"oversize"
#define ResPort			"port"
#define ResPrinterCommand	"printer.command"
#define ResQuit			"quit"
#define ResReconnect		"reconnect"
#define ResRectangleSelect	"rectangleSelect"
#define ResRprnt		"rprnt"
#define ResSaveLines		"saveLines"
#define ResSchemeList		"schemeList"
#define ResScreenTrace		"screenTrace"
#define ResScreenTraceFile	"screenTraceFile"
#define ResScripted		"scripted"
#define ResScrollBar		"scrollBar"
#define ResSecure		"secure"
#define ResSelectBackground	"selectBackground"
#define ResShowTiming		"showTiming"
#define ResTermName		"termName"
#define ResTraceDir		"traceDir"
#define ResTraceFile		"traceFile"
#define ResTypeahead		"typeahead"
#define ResUseCursorColor	"useCursorColor"
#define ResVisualBell		"visualBell"
#define ResVisualSelect		"visualSelect"
#define ResVisualSelectColor	"visualSelectColor"
#define ResWaitCursor		"waitCursor"
#define ResWerase		"werase"

/* Dotted resource names. */
#define DotActiveIcon		"." ResActiveIcon
#define DotAplMode		"." ResAplMode
#define DotCharClass		"." ResCharClass
#define DotCharset		"." ResCharset
#define DotColorScheme		"." ResColorScheme
#define DotDsTrace		"." ResDsTrace
#define DotEmulatorFont		"." ResEmulatorFont
#define DotExtended		"." ResExtended
#define DotKeymap		"." ResKeymap
#define DotKeypadOn		"." ResKeypadOn
#define DotM3279		"." ResM3279
#define DotModel		"." ResModel
#define DotMono			"." ResMono
#define DotOnce			"." ResOnce
#define DotOversize		"." ResOversize
#define DotPort			"." ResPort
#define DotReconnect		"." ResReconnect
#define DotSaveLines		"." ResSaveLines
#define DotScripted		"." ResScripted
#define DotScrollBar		"." ResScrollBar
#define DotTermName		"." ResTermName
#define DotTraceFile		"." ResTraceFile

/* Resource classes. */
#define ClsActiveIcon		"ActiveIcon"
#define ClsAdVersion		"AdVersion"
#define ClsAllowResize		"AllowResize"
#define ClsAltCursor		"AltCursor"
#define ClsAplFont		"AplFont"
#define ClsAplMode		"AplMode"
#define ClsBaselevelTranslations	"BaselevelTranslations"
#define ClsBellVolume		"BellVolume"
#define ClsBlankFill		"BlankFill"
#define ClsBoldColor		"BoldColor"
#define ClsCharClass		"CharClass"
#define ClsCharset		"Charset"
#define ClsColorBackground	"ColorBackground"
#define ClsColorScheme		"ColorScheme"
#define ClsComposeMap		"ComposeMap"
#define ClsConnectFileName	"ConnectFileName"
#define ClsCursorBlink		"CursorBlink"
#define ClsCursorColor		"CursorColor"
#define ClsCursorPos		"CursorPos"
#define ClsDebugFont		"DebugFont"
#define ClsDebugTracing		"DebugTracing"
#define ClsDisconnectClear	"DisconnectClear"
#define ClsDoConfirms		"DoConfirms"
#define ClsDsTrace		"DsTrace"
#define ClsEmulatorFont		"EmulatorFont"
#define ClsEof			"Eof"
#define ClsErase		"Erase"
#define ClsEventTrace		"EventTrace"
#define ClsExtended		"Extended"
#define ClsFontList		"FontList"
#define ClsFtCommand		"FtCommand"
#define ClsHighlightBold	"HighlightBold"
#define ClsHighlightSelect	"HighlightSelect"
#define ClsHostsFile		"HostsFile"
#define ClsIconFont		"IconFont"
#define ClsIconLabelFont	"IconLabelFont"
#define ClsIcrnl		"Icrnl"
#define ClsInlcr		"Inlcr"
#define ClsInputColor		"InputColor"
#define ClsIntr			"Intr"
#define ClsInvertKeypadShift	"InvertKeypadShift"
#define ClsKeymap		"Keymap"
#define ClsKeypad		"Keypad"
#define ClsKeypadBackground	"KeypadBackground"
#define ClsKeypadOn		"KeypadOn"
#define ClsKill			"Kill"
#define ClsLabelIcon		"LabelIcon"
#define ClsLineWrap		"LineWrap"
#define ClsLnext		"Lnext"
#define ClsLockedCursor		"LockedCursor"
#define ClsM3279		"M3279"
#define ClsMacros		"Macros"
#define ClsMarginedPaste	"MarginedPaste"
#define ClsMenuBar		"MenuBar"
#define ClsModel		"Model"
#define ClsModifiedSel		"ModifiedSel"
#define ClsModifiedSelColor	"ModifiedSelColor"
#define ClsMono			"Mono"
#define ClsMonoCase		"MonoCase"
#define ClsNoOther		"NoOther"
#define ClsNormalColor		"NormalColor"
#define ClsNormalCursor		"NormalCursor"
#define ClsNumericLock		"NumericLock"
#define ClsOerrLock		"OerrLock"
#define ClsOnce			"Once"
#define ClsOversize		"Oversize"
#define ClsPort			"Port"
#define ClsQuit			"Quit"
#define ClsReconnect		"Reconnect"
#define ClsRectangleSelect	"RectangleSelect"
#define ClsRprnt		"Rprnt"
#define ClsSaveLines		"SaveLines"
#define ClsScreenTrace		"ScreenTrace"
#define ClsScreenTraceFile	"ScreenTraceFile"
#define ClsScripted		"Scripted"
#define ClsScrollBar		"ScrollBar"
#define ClsSecure		"Secure"
#define ClsSelectBackground	"SelectBackground"
#define ClsShowTiming		"ShowTiming"
#define ClsTermName		"TermName"
#define ClsTraceDir		"TraceDir"
#define ClsTraceFile		"TraceFile"
#define ClsTypeahead		"Typeahead"
#define ClsUseCursorColor	"UseCursorColor"
#define ClsVisualBell		"VisualBell"
#define ClsVisualSelect		"VisualSelect"
#define ClsVisualSelectColor	"VisualSelectColor"
#define ClsWaitCursor		"WaitCursor"
#define ClsWerase		"Werase"

/* Options. */
#define OptActiveIcon		"-activeicon"
#define OptAplMode		"-apl"
#define OptCharClass		"-cc"
#define OptCharset		"-charset"
#define OptClear		"-clear"
#define OptColorScheme		"-scheme"
#define OptDsTrace		"-trace"
#define OptEmulatorFont		"-efont"
#define OptExtended		"-extended"
#define OptHostsFile		"-hostsfile"
#define OptIconName		"-iconname"
#define OptIconX		"-iconx"
#define OptIconY		"-icony"
#define OptKeymap		"-keymap"
#define OptKeypadOn		"-keypad"
#define OptLocalProcess		"-e"
#define OptM3279		"-color"
#define OptModel		"-model"
#define OptMono			"-mono"
#define OptNoScrollBar		"+sb"
#define OptOnce			"-once"
#define OptOversize		"-oversize"
#define OptPort			"-port"
#define OptReconnect		"-reconnect"
#define OptSaveLines		"-sl"
#define OptScripted		"-script"
#define OptScrollBar		"-sb"
#define OptSet			"-set"
#define OptTermName		"-tn"
#define OptTraceFile		"-tracefile"

/* Miscellaneous values. */
#define ResTrue			"true"
#define ResFalse		"false"
#define KpLeft			"left"
#define KpRight			"right"
#define KpBottom		"bottom"
#define KpIntegral		"integral"
#define Apl			"apl"

/* Resources that are gotten explicitly. */
#define ResNvt			"nvt"
#define ResPrintTextCommand	"printTextCommand"
#define ResPrintWindowCommand	"printWindowCommand"
#define ResResizeFontList	"resizeFontList"
#define ResKeyHeight		"keyHeight"
#define ResKeyWidth		"keyWidth"
#define ResPfWidth		"pfWidth"
#define ResPaWidth		"paWidth"
#define ResLargeKeyWidth	"largeKeyWidth"
#define ResComposeMap		"composeMap"
#define ResTraceCommand		"traceCommand"
#define ResMessage		"message"
#define ResUser			"user"
