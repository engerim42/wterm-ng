# Generated automatically from Makefile.in by configure.
# src/Makefile.in	-*- Makefile -*-
# $Id: Makefile.in,v 1.5 1998/09/22 17:29:40 mason Exp $

# autoconf/Make.common.in	 -*- Makefile -*-
# release date (man), LSM date, version number/name, current maintainer
# $Id: Make.common.in,v 1.2 1998/04/20 07:24:15 mason Exp $
DATE=25 AUGUST 2001
LSMDATE=25AUG01
VERSION=6.2.9
VERNAME=wterm-$(VERSION)#
MAINT=an anonymous coder#
MAINTEMAIL=<largo@current.nu>#
WEBMAINT=Largo#
WEBMAINTEMAIL=<largo@current.nu>#
WEBPAGE=<http://wm.current.nu/files.php#wterm>#
FTPSITENAME=ftp.current.nu#
FTPSITEDIR=/pub/WindowMaker#
#-------------------------------------------------------------------------

SHELL = /bin/sh

# This variable makes it possible to move the installation root to another
# directory. This is useful when you're creating a binary distribution
# If empty, normal root will be used.
# You can run eg. 'make install DESTDIR=/packages/wterm-xx' to accomplish
# that.
# DESTDIR = /usr/local/X11/$(VERNAME)

# Installation target directories & other installation stuff
prefix = /usr/local
exec_prefix = ${prefix}
binprefix =
manprefix =
bindir = ${exec_prefix}/bin
mandir = ${prefix}/man/man1
manext = 1

# Tools & program stuff
CC = gcc
CPP = gcc -E
MV = /bin/mv
RM = /bin/rm
RMF = /bin/rm -f
CP = /bin/cp
SED = /usr/bin/sed
INSTALL = /usr/bin/ginstall -c
INSTALL_PROGRAM = /usr/bin/ginstall -c -s -m 755
INSTALL_DATA = /usr/bin/ginstall -c -m 644

# Flags & libs
# add -DBINDIR=\""$(bindir)/"\" to CPPFLAGS, if we need to spawn a program

CFLAGS = -O
CPPFLAGS =  
LDFLAGS = 
DEFS = -DHAVE_CONFIG_H
LIBS = 
DINCLUDE = 
DLIB = 

# X Include directory
XINC =  -I/usr/X11R6/include 

# extra libraries needed by X on some systems, X library location
XLIB =  -L/usr/X11R6/lib -L/usr/local/lib -lWMaker -lWINGs -lwraster -lPropList -L/usr/X11R6/lib -lXpm -lm -ljpeg -lpng -lz -ltiff -lgif -lXext -lX11 
# End of common section of the Makefile
#-------------------------------------------------------------------------

srcdir = .

basedir = ..
thisdir = src

# for developers: the following debug options may be used
#	-DDEBUG_CMD -DDEBUG_MENU -DDEBUG_MENUARROWS -DDEBUG_MENUBAR_STACKING
#	-DDEBUG_MENU_LAYOUT -DDEBUG_RESOURCES -DDEBUG_SCREEN
#	-DDEBUG_SEARCH_PATH -DDEBUG_SELECT -DDEBUG_TTYMODE -DDEBUG_X
DEBUG=-DDEBUG_STRICT 

first_rule: all
dummy:

SRCS =	command.c graphics.c grkelot.c main.c menubar.c misc.c \
	netdisp.c rmemset.c screen.c scrollbar2.c utmp.c xdefaults.c xpm.c

OBJS =	command.o graphics.o grkelot.o main.o menubar.o misc.o \
	netdisp.o rmemset.o screen.o scrollbar2.o utmp.o xdefaults.o xpm.o

HDRS =	feature.h protos.h grkelot.h wterm.h wtermgrx.h screen.h version.h

PROS =	command.pro graphics.pro grkelot.pro main.pro menubar.pro misc.pro \
	netdisp.pro rmemset.pro screen.pro scrollbar2.pro utmp.pro xdefaults.pro xpm.pro

DEPS =  wterm.h version.h ${basedir}/config.h feature.h

#
# Distribution variables
#

DIST = $(HDRS) $(SRCS) Makefile.in gcc-Wall .indent.pro makeprotos-sed

.SUFFIXES: .c .o .pro

# inference rules
.c.o:
	$(CC) $(DEFS) $(DEBUG) -c $(CPPFLAGS) $(XINC)  -I. -I$(basedir) -I$(srcdir) $(DINCLUDE) $(CFLAGS) $<
#-------------------------------------------------------------------------
all: wterm

wterm: version.h $(PROS) $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS) $(XLIB) $(DLIB)

.c.pro:
	$(SED) -n -f $(srcdir)/makeprotos-sed $< > $@

tags: $(SRCS)
	ctags $(SRCS)

allbin: wterm

alldoc:

clean:
	$(RMF) wterm core a.out $(OBJS) *.bak *~

realclean: clean
	$(RMF) tags *.pro

distclean: realclean
	if test $(srcdir) = .; then $(MAKE) realclean; fi
	(cd $(srcdir); $(RMF) Makefile)

install: allbin alldoc
	$(INSTALL_PROGRAM) wterm $(DESTDIR)$(bindir)/$(binprefix)wterm

uninstall:
	(cd $(bindir); $(RMF) $(binprefix)wterm)

distdirs:
	mkdir $(basedir)/../$(VERNAME)/$(thisdir)

distcopy:
	$(CP) -p $(DIST) $(basedir)/../$(VERNAME)/$(thisdir)

# Semi-automatic generation of dependencies:
# Use gcc -MM because X11 `makedepend' doesn't work on all systems
# and it also includes system headers.
# `semi'-automatic since dependencies are generated at distribution time.

#distdepend:
#	mv Makefile.in Makefile.in~
#	sed "/^# DO NOT DELETE:/,$$ d" Makefile.in~ > Makefile.in
#	echo "# DO NOT DELETE: ugly dependency list follows" >> Makefile.in
#	gcc -MM $(CPPFLAGS) $(XINC) -I. -I$(basedir) -I$(srcdir) $(SRCS) >> Makefile.in

# -----------------------------------------------------------------------
# DO NOT DELETE: nice dependency list follows
command.o:   command.c   command.pro   $(DEPS)
graphics.o:  graphics.c  graphics.pro  $(DEPS)
grkelot.o:   grkelot.c   grkelot.pro   $(DEPS)
main.o:      main.c      main.pro      $(DEPS)
menubar.o:   menubar.c   menubar.pro   $(DEPS)
misc.o:      misc.c      misc.pro      $(DEPS)
netdisp.o:   netdisp.c   netdisp.pro   $(DEPS)
screen.o:    screen.c    screen.pro    $(DEPS) screen.h
scrollbar2.o: scrollbar2.c scrollbar2.pro $(DEPS)
utmp.o:      utmp.c      utmp.pro      $(DEPS)
xdefaults.o: xdefaults.c xdefaults.pro $(DEPS)
xpm.o:       xpm.c       xpm.pro       $(DEPS)
