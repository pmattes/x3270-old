/*
 * Copyright (c) 1995-2009, Paul Mattes.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the names of Paul Mattes nor the names of his contributors
 *       may be used to endorse or promote products derived from this software
 *       without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL MATTES "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL PAUL MATTES BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *	resources.h
 *		x3270/c3270/s3270/tcl3270 resource and option names.
 */

/* Resources. */
#define ResAcs			"acs"
#define ResActiveIcon		"activeIcon"
#define ResAdVersion		"adVersion"
#define ResAidWait		"aidWait"
#define ResAllBold		"allBold"
#define ResAllowResize		"allowResize"
#define ResAltCursor		"altCursor"
#define ResAltScreen		"altScreen"
#define ResAplMode		"aplMode"
#define ResAsciiBoxDraw		"asciiBoxDraw"
#define ResAssocCommand		"printer.assocCommandLine"
#define ResBaselevelTranslations	"baselevelTranslations"
#define ResBellVolume		"bellVolume"
#define ResBlankFill		"blankFill"
#define ResBoldColor		"boldColor"
#define ResBsdTm		"bsdTm"
#define ResCbreak		"cbreak"
#define ResCertFile		"certFile"
#define ResCharClass		"charClass"
#define ResCharset		"charset"
#define ResCharsetList		"charsetList"
#define ResColor8		"color8"
#define ResColorBackground	"colorBackground"
#define ResColorScheme		"colorScheme"
#define ResCommandTimeout	"commandTimeout"
#define ResComposeMap		"composeMap"
#define ResConfDir		"confDir"
#define ResConnectFileName	"connectFileName"
#define ResConsoleColorForHostColor "consoleColorForHostColor"
#define ResCrosshair		"crosshair"
#define ResCursesColorFor	"cursesColorFor"
#define ResCursesColorForDefault ResCursesColorFor "Default"
#define ResCursesColorForHostColor ResCursesColorFor "HostColor"
#define ResCursesColorForIntensified ResCursesColorFor "Intensified"
#define ResCursesColorForProtected ResCursesColorFor "Protected"
#define ResCursesColorForProtectedIntensified ResCursesColorFor "ProtectedIntensified"
#define ResCursesKeypad		"cursesKeypad"
#define ResCursorBlink		"cursorBlink"
#define ResCursorColor		"cursorColor"
#define ResCursorPos		"cursorPos"
#define ResDebugTracing		"debugTracing"
#define ResDefScreen		"defScreen"
#define ResDftBufferSize	"dftBufferSize"
#define ResDisconnectClear	"disconnectClear"
#define ResDoConfirms		"doConfirms"
#define ResDbcsCgcsgid		"dbcsCgcsgid"
#define ResDsTrace		"dsTrace"
#define ResEmulatorFont		"emulatorFont"
#define ResEof			"eof"
#define ResErase		"erase"
#define ResEventTrace		"eventTrace"
#define ResExtended		"extended"
#define ResFixedSize		"fixedSize"
#define ResHighlightBold	"highlightBold"
#define ResHighlightUnderline	"highlightUnderline"
#define ResHostColorFor		"hostColorFor"
#define ResHostColorForDefault ResHostColorFor "Default"
#define ResHostColorForIntensified ResHostColorFor "Intensified"
#define ResHostColorForProtected ResHostColorFor "Protected"
#define ResHostColorForProtectedIntensified ResHostColorFor "ProtectedIntensified"
#define ResHostname		"hostname"
#define ResHostsFile		"hostsFile"
#define ResIconFont		"iconFont"
#define ResIconLabelFont	"iconLabelFont"
#define ResIcrnl		"icrnl"
#define ResIdleCommand		"idleCommand"
#define ResIdleCommandEnabled	"idleCommandEnabled"
#define ResIdleTimeout		"idleTimeout"
#define ResInlcr		"inlcr"
#define ResInputColor		"inputColor"
#define ResInputMethod		"inputMethod"
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
#define ResLocalCp		"localCp"
#define ResLoginMacro		"loginMacro"
#define ResLockedCursor		"lockedCursor"
#define ResLuCommandLine	"printer.luCommandLine"
#define ResM3279		"m3279"
#define ResMacros		"macros"
#define ResMarginedPaste	"marginedPaste"
#define ResMenuBar		"menuBar"
#define ResMetaEscape		"metaEscape"
#define ResModel		"model"
#define ResModifiedSel		"modifiedSel"
#define ResModifiedSelColor	"modifiedSelColor"
#define ResMono			"mono"
#define ResMonoCase		"monoCase"
#define ResNoOther		"noOther"
#define ResNoPrompt		"noPrompt"
#define ResNormalColor		"normalColor"
#define ResNormalCursor		"normalCursor"
#define ResNumericLock		"numericLock"
#define ResOerrLock		"oerrLock"
#define ResOnce			"once"
#define ResOnlcr		"onlcr"
#define ResOversize		"oversize"
#define ResPluginCommand	"pluginCommand"
#define ResPort			"port"
#define ResPreeditType		"preeditType"
#define ResPrinterCodepage	"printer.codepage"
#define ResPrinterCommand	"printer.command"
#define ResPrinterLu		"printerLu"
#define ResPrinterName		"printer.name"

#define ResProxy		"proxy"
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
#define ResSbcsCgcsgid		"sbcsCgcsgid"
#define ResShowTiming		"showTiming"
#define ResSocket		"socket"
#define ResSuppressActions	"suppressActions"
#define ResSuppressHost		"suppressHost"
#define ResSuppressFontMenu	"suppressFontMenu"
#define ResSuppress		"suppress"
#define ResTermName		"termName"
#define ResTitle		"title"
#define ResTraceDir		"traceDir"
#define ResTraceFile		"traceFile"
#define ResTraceFileSize	"traceFileSize"
#define ResTraceMonitor		"traceMonitor"
#define ResTypeahead		"typeahead"
#define ResUnderscore		"underscore"
#define ResUnlockDelay		"unlockDelay"
#define ResUnlockDelayMs	"unlockDelayMs"
#define ResUseCursorColor	"useCursorColor"
#define ResV			"v"
#define ResVisibleControl	"visibleControl"
#define ResVisualBell		"visualBell"
#define ResVisualSelect		"visualSelect"
#define ResVisualSelectColor	"visualSelectColor"
#define ResWaitCursor		"waitCursor"
#define ResWerase		"werase"

/* Dotted resource names. */
#define DotActiveIcon		"." ResActiveIcon
#define DotAplMode		"." ResAplMode
#define DotCertFile		"." ResCertFile
#define DotCbreak		"." ResCbreak
#define DotCharClass		"." ResCharClass
#define DotCharset		"." ResCharset
#define DotColorScheme		"." ResColorScheme
#define DotDsTrace		"." ResDsTrace
#define DotEmulatorFont		"." ResEmulatorFont
#define DotExtended		"." ResExtended
#define DotInputMethod		"." ResInputMethod
#define DotKeymap		"." ResKeymap
#define DotKeypadOn		"." ResKeypadOn
#define DotM3279		"." ResM3279
#define DotModel		"." ResModel
#define DotMono			"." ResMono
#define DotOnce			"." ResOnce
#define DotOversize		"." ResOversize
#define DotPort			"." ResPort
#define DotPreeditType		"." ResPreeditType
#define DotPrinterLu		"." ResPrinterLu
#define DotProxy		"." ResProxy
#define DotReconnect		"." ResReconnect
#define DotSaveLines		"." ResSaveLines
#define DotScripted		"." ResScripted
#define DotScrollBar		"." ResScrollBar
#define DotSocket		"." ResSocket
#define DotTermName		"." ResTermName
#define DotTitle		"." ResTitle
#define DotTraceFile		"." ResTraceFile
#define DotTraceFileSize	"." ResTraceFileSize
#define DotV			"." ResV

/* Resource classes. */
#define ClsActiveIcon		"ActiveIcon"
#define ClsAdVersion		"AdVersion"
#define ClsAidWait		"AidWait"
#define ClsAllBold		"AllBold"
#define ClsAllowResize		"AllowResize"
#define ClsAltCursor		"AltCursor"
#define ClsAplMode		"AplMode"
#define ClsBaselevelTranslations	"BaselevelTranslations"
#define ClsBellVolume		"BellVolume"
#define ClsBlankFill		"BlankFill"
#define ClsBoldColor		"BoldColor"
#define ClsBsdTm		"BsdTm"
#define ClsCbreak		"Cbreak"
#define ClsCertFile		"CertFile"
#define ClsCharClass		"CharClass"
#define ClsCharset		"Charset"
#define ClsColor8		"Color8"
#define ClsColorBackground	"ColorBackground"
#define ClsColorScheme		"ColorScheme"
#define ClsComposeMap		"ComposeMap"
#define ClsConfDir		"ConfDir"
#define ClsConnectFileName	"ConnectFileName"
#define ClsCrosshair		"Crosshair"
#define ClsCursorBlink		"CursorBlink"
#define ClsCursorColor		"CursorColor"
#define ClsCursorPos		"CursorPos"
#define ClsDbcsCgcsgid		"DbcsCgcsgid"
#define ClsDebugTracing		"DebugTracing"
#define ClsDftBufferSize	"DftBufferSize"
#define ClsDisconnectClear	"DisconnectClear"
#define ClsDoConfirms		"DoConfirms"
#define ClsDsTrace		"DsTrace"
#define ClsEmulatorFont		"EmulatorFont"
#define ClsEof			"Eof"
#define ClsErase		"Erase"
#define ClsEventTrace		"EventTrace"
#define ClsExtended		"Extended"
#define ClsFixedSize		"FixedSize"
#define ClsFtCommand		"FtCommand"
#define ClsHighlightBold	"HighlightBold"
#define ClsHostname		"Hostname"
#define ClsHostsFile		"HostsFile"
#define ClsIconFont		"IconFont"
#define ClsIconLabelFont	"IconLabelFont"
#define ClsIcrnl		"Icrnl"
#define ClsIdleCommand		"IdleCommand"
#define ClsIdleCommandEnabled	"IdleCommandEnabled"
#define ClsIdleTimeout		"IdleTimeout"
#define ClsInlcr		"Inlcr"
#define ClsInputColor		"InputColor"
#define ClsInputMethod		"InputMethod"
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
#define ClsMetaEscape		"MetaEscape"
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
#define ClsOnlcr		"Onlcr"
#define ClsOversize		"Oversize"
#define ClsPluginCommand	"PluginCommand"
#define ClsPort			"Port"
#define ClsPreeditType		"PreeditType"
#define ClsPrinterLu		"PrinterLu"
#define ClsProxy		"Proxy"
#define ClsQuit			"Quit"
#define ClsReconnect		"Reconnect"
#define ClsRectangleSelect	"RectangleSelect"
#define ClsRprnt		"Rprnt"
#define ClsSaveLines		"SaveLines"
#define ClsSbcsCgcsgid		"SbcsSgcsgid"
#define ClsScreenTrace		"ScreenTrace"
#define ClsScreenTraceFile	"ScreenTraceFile"
#define ClsScripted		"Scripted"
#define ClsScrollBar		"ScrollBar"
#define ClsSecure		"Secure"
#define ClsSelectBackground	"SelectBackground"
#define ClsShowTiming		"ShowTiming"
#define ClsSocket		"Socket"
#define ClsSuppressHost		"SuppressHost"
#define ClsSuppressFontMenu	"SuppressFontMenu"
#define ClsTermName		"TermName"
#define ClsTraceDir		"TraceDir"
#define ClsTraceFile		"TraceFile"
#define ClsTraceFileSize	"TraceFileSize"
#define ClsTraceMonitor		"TraceMonitor"
#define ClsTypeahead		"Typeahead"
#define ClsUnlockDelay		"UnlockDelay"
#define ClsUnlockDelayMs	"UnlockDelayMs"
#define ClsUseCursorColor	"UseCursorColor"
#define ClsVisibleControl	"VisibleControl"
#define ClsVisualBell		"VisualBell"
#define ClsVisualSelect		"VisualSelect"
#define ClsVisualSelectColor	"VisualSelectColor"
#define ClsWaitCursor		"WaitCursor"
#define ClsWerase		"Werase"

/* Options. */
#define OptActiveIcon		"-activeicon"
#define OptAllBold		"-allbold"
#define OptAltScreen		"-altscreen"
#define OptAplMode		"-apl"
#define OptCbreak		"-cbreak"
#define OptCertFile		"-certificate"
#define OptCharClass		"-cc"
#define OptCharset		"-charset"
#define OptClear		"-clear"
#define OptColorScheme		"-scheme"
#define OptDefScreen		"-defscreen"
#define OptDsTrace		"-trace"
#define OptEmulatorFont		"-efont"
#define OptExtended		"-extended"
#define OptHostsFile		"-hostsfile"
#define OptIconName		"-iconname"
#define OptIconX		"-iconx"
#define OptIconY		"-icony"
#define OptInputMethod		"-im"
#define OptKeymap		"-keymap"
#define OptKeypadOn		"-keypad"
#define OptLocalCp		"-localcp"
#define OptLocalProcess		"-e"
#define OptM3279		"-color"
#define OptModel		"-model"
#define OptMono			"-mono"
#define OptNoPrompt		"-noprompt"
#define OptNoScrollBar		"+sb"
#define OptOnce			"-once"
#define OptOversize		"-oversize"
#define OptPort			"-port"
#define OptPreeditType		"-pt"
#define OptPrinterLu		"-printerlu"
#define OptProxy		"-proxy"
#define OptReconnect		"-reconnect"
#define OptSaveLines		"-sl"
#define OptSecure		"-secure"
#define OptScripted		"-script"
#define OptScrollBar		"-sb"
#define OptSet			"-set"
#define OptSocket		"-socket"
#define OptTermName		"-tn"
#define OptTitle		"-title"
#define OptTraceFile		"-tracefile"
#define OptTraceFileSize	"-tracefilesize"
#define OptV			"-v"
#define OptVersion		"--version"

/* Miscellaneous values. */
#define ResTrue			"true"
#define ResFalse		"false"
#define KpLeft			"left"
#define KpRight			"right"
#define KpBottom		"bottom"
#define KpIntegral		"integral"
#define KpInsideRight		"insideRight"
#define Apl			"apl"

/* Resources that are gotten explicitly. */
#define ResComposeMap		"composeMap"
#define ResEmulatorFontList	"emulatorFontList"
#define ResKeyHeight		"keyHeight"
#define ResKeyWidth		"keyWidth"
#define ResLargeKeyWidth	"largeKeyWidth"
#define ResMessage		"message"
#define ResNvt			"nvt"
#define ResPaWidth		"paWidth"
#define ResPfWidth		"pfWidth"
#define ResPrintTextCommand	"printTextCommand"
#define ResPrintTextFont	"printTextFont"
#define ResPrintTextSize	"printTextSize"
#define ResPrintWindowCommand	"printWindowCommand"
#define ResTraceCommand		"traceCommand"
#define ResUser			"user"
