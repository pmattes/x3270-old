#
# Copyright 1989 by Georgia Tech Research Corporation, Atlanta, GA.
# Copyright 1988, 1989 by Robert Viduya.
#
#                         All Rights Reserved
#
#
# Things to check/change.
#
#	FONTDIR		the location of where the custom font should be.
#	BINDIR		destination of the binary
#	MANDIR		destination of the manual page
#	MANEXT		the . extension of the installed manual page
#	CFLAGS		add -DTYPE4KBD if you have a Type 4 kybd
#
FONTDIR	= /usr/local/lib/fonts
BINDIR	= /usr/local/bin
MANDIR	= /usr/man/manl
MANEXT	= l
CC	= cc
CFLAGS	= -O2 -DTYPE4KBD -DFONT3270=\"$(FONTDIR)/3270.font\"
#CFLAGS	= -DDEBUG -DFONT3270=\"$(FONTDIR)/3270.font\"

3270tool:	3270tool.o screen.o telnet.o kybd.o keypanel.o selection.o
	$(CC) $(CFLAGS) -o 3270tool 3270tool.o screen.o telnet.o kybd.o keypanel.o selection.o -lsuntool -lsunwindow -lpixrect

install: $(BINDIR)/3270tool $(MANDIR)/3270tool.$(MANEXT) $(FONTDIR)/3270.font

$(BINDIR)/3270tool:	3270tool
	install 3270tool $(BINDIR)

$(MANDIR)/3270tool.$(MANEXT):	3270tool.1
	install 3270tool.1 $(MANDIR)/3270tool.$(MANEXT)

$(FONTDIR)/3270.font:	3270.font
	install 3270.font $(FONTDIR)

3270tool.o:	3270tool.c 3270.icon
	$(CC) $(CFLAGS) -c 3270tool.c

screen.o:	screen.c 3270.h
	$(CC) $(CFLAGS) -c screen.c

telnet.o:	telnet.c
	$(CC) $(CFLAGS) -c telnet.c

kybd.o:	kybd.c 3270.h 3270_enc.h
	$(CC) $(CFLAGS) -c kybd.c

keypanel.o:	keypanel.c	\
		BtnPA1 BtnPA2 BtnClear BtnPF01 BtnPF02 BtnPF03 \
		BtnPF04 BtnPF05 BtnPF06 BtnPF07 BtnPF08 BtnPF09 \
		BtnPF10 BtnPF11 BtnPF12 BtnPF13 BtnPF14 BtnPF15 \
		BtnPF16 BtnPF17 BtnPF18 BtnPF19 BtnPF20 BtnPF21 \
		BtnPF22 BtnPF23 BtnPF24 BtnSysReq BtnEnter BtnShift
	$(CC) $(CFLAGS) -c keypanel.c

selection.o:	selection.c 3270.h 3270_enc.h
	$(CC) $(CFLAGS) -c selection.c
