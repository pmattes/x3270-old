/*
 * Copyright 1995, 1996, 1999 by Paul Mattes.
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
#define ResAttnLock		"attnLock"
#define ResBaselevelTranslations	"baselevelTranslations"
#define ResBellVolume		"bellVolume"
#define ResBlankFill		"blankFill"
#define ResBoldColor		"boldColor"
#define ResCharClass		"charClass"
#define ResCharset		"charset"
#define ResCharsetList		"charsetList"
#define ResColorBackground	"colorBackground"
#define ResColorScheme		"colorScheme"
#define ResComposeMap		"composeMap"
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
#define ResFontList		"fontList"
#define ResFtCommand		"ftCommand"
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
#define ResNumericLock		"numericLock"
#define ResOerrLock		"oerrLock"
#define ResOnce			"once"
#define ResOversize		"oversize"
#define ResPort			"port"
#define ResQuit			"quit"
#define ResReconnect		"reconnect"
#define ResRectangleSelect	"rectangleSelect"
#define ResRprnt		"rprnt"
#define ResSaveLines		"saveLines"
#define ResSchemeList		"schemeList"
#define ResScreenTrace		"screenTrace"
#define ResScripted		"scripted"
#define ResScrollBar		"scrollBar"
#define ResSecure		"secure"
#define ResSelectBackground	"selectBackground"
#define ResShowTiming		"showTiming"
#define ResTermName		"termName"
#define ResTraceDir		"traceDir"
#define ResTypeahead		"typeahead"
#define ResUseCursorColor	"useCursorColor"
#define ResVisualBell		"visualBell"
#define ResWaitCursor		"waitCursor"
#define ResWerase		"werase"

/* Dotted resource names. */
#define DotActiveIcon		".activeIcon"
#define DotAplMode		".aplMode"
#define DotCharClass		".charClass"
#define DotCharset		".charset"
#define DotColorScheme		".colorScheme"
#define DotDsTrace		".dsTrace"
#define DotEmulatorFont		".emulatorFont"
#define DotExtended		".extended"
#define DotKeymap		".keymap"
#define DotKeypadOn		".keypadOn"
#define DotM3279		".m3279"
#define DotModel		".model"
#define DotMono			".mono"
#define DotOnce			".once"
#define DotOversize		".oversize"
#define DotPort			".port"
#define DotReconnect		".reconnect"
#define DotSaveLines		".saveLines"
#define DotScripted		".scripted"
#define DotScrollBar		".scrollBar"
#define DotTermName		".termName"

/* Resource classes. */
#define ClsActiveIcon		"ActiveIcon"
#define ClsAdVersion		"AdVersion"
#define ClsAllowResize		"AllowResize"
#define ClsAltCursor		"AltCursor"
#define ClsAplFont		"AplFont"
#define ClsAplMode		"AplMode"
#define ClsAttnLock		"AttnLock"
#define ClsBaselevelTranslations	"BaselevelTranslations"
#define ClsBellVolume		"BellVolume"
#define ClsBlankFill		"BlankFill"
#define ClsBoldColor		"BoldColor"
#define ClsCharClass		"CharClass"
#define ClsCharset		"Charset"
#define ClsColorBackground	"ColorBackground"
#define ClsColorScheme		"ColorScheme"
#define ClsComposeMap		"ComposeMap"
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
#define ClsScripted		"Scripted"
#define ClsScrollBar		"ScrollBar"
#define ClsSecure		"Secure"
#define ClsSelectBackground	"SelectBackground"
#define ClsShowTiming		"ShowTiming"
#define ClsTermName		"TermName"
#define ClsTraceDir		"TraceDir"
#define ClsTypeahead		"Typeahead"
#define ClsUseCursorColor	"UseCursorColor"
#define ClsVisualBell		"VisualBell"
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

/* Miscellaneous values. */
#define ResTrue			"true"
#define ResFalse		"false"
#define KpLeft			"left"
#define KpRight			"right"
#define KpBottom		"bottom"
#define KpIntegral		"integral"
#define Apl			"apl"

/* Resources that are gotten explicitly. */
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
