# Generated automatically from Makefile.in by configure.
# doc/Makefile.in	-*- Makefile -*-
# $Id: Makefile.in,v 1.5 1998/09/20 23:49:22 mason Exp $

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
WEBPAGE=<http://wm.current.nu/files.html#wterm>#
FTPSITENAME=ftp.dct.com#
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
XLIB =  -L/usr/X11R6/lib  -lSM -lICE  -L/usr/X11/lib -lXpm -lX11 -lWMaker

# End of common section of the Makefile
#-------------------------------------------------------------------------

srcdir = .

basedir = ..
thisdir = doc

first_rule: all
dummy:

#
# Distribution variables
#


DIST_ETC  = etc/wterm.termcap etc/wterm.terminfo etc/XTerm.ad
DIST_MENU = menu/menu menu/example.menu menu/wterm.menu menu/terminal.menu\
	    menu/jedmenu.sl
DIST	  = Makefile.in README.greek README.menu README.xvt BUGS FAQ TODO\
	    changes.txt xterm.seq LSM.in wterm.1 wterm.pretbl wtermRef.txt\
	    wtermRef.html wtermRef-frame.html wtermRef-toc.html wterm.html 

#-------------------------------------------------------------------------

all:

SEDREPLACE = -e 's%@WTERM_VERSION@%$(VERSION)%g;'\
	     -e 's%@WTERM_LSMDATE@%$(LSMDATE)%g;'\
	     -e 's%@WTERM_DATE@%$(DATE)%g;'\
	     -e 's%@WTERM_MAINTEMAIL@%$(MAINTEMAIL)%g;'\
	     -e 's%@WTERM_MAINT@%$(MAINT)%g;'\
	     -e 's%@WTERM_WEBPAGE@%$(WEBPAGE)%g;'\
	     -e 's%@WTERM_WEBMAINTEMAIL@%$(WEBMAINTEMAIL)%g;'\
	     -e 's%@WTERM_WEBMAINT@%$(WEBMAINT)%g;'\
	     -e 's%@WTERM_FTPSITENAME@%$(FTPSITENAME)%g;'\
	     -e 's%@WTERM_FTPSITEDIR@%$(FTPSITEDIR)%g;'


tags allbin:

alldoc:	$(basedir)/$(VERNAME).lsm wterm.1 wterm.html wtermRef.html wtermRef.txt

$(basedir)/$(VERNAME).lsm: LSM.in ../autoconf/Make.common.in
	$(SED) $(SEDREPLACE) < LSM.in > $@

wterm.1: wterm.pretbl
	tbl wterm.pretbl > $@
clean:
	$(RMF) wterm.man *~ 

realclean: clean
	$(RMF) tags wterm.1 wterm.html wtermRef.html wtermRef.txt

distclean:

install: wterm.1
	$(INSTALL_DATA) wterm.1 $(DESTDIR)$(mandir)/$(manprefix)wterm.$(manext)

uninstall:
	-cd $(mandir); $(RMF) $(manprefix)wterm.$(manext)

distdirs:
	mkdir $(basedir)/../$(VERNAME)/$(thisdir)
	mkdir $(basedir)/../$(VERNAME)/$(thisdir)/etc
	mkdir $(basedir)/../$(VERNAME)/$(thisdir)/menu

distcopy: distdepend
	$(CP) -p $(basedir)/$(VERNAME).lsm $(basedir)/../$(VERNAME);
	$(CP) -p $(DIST)      $(basedir)/../$(VERNAME)/$(thisdir)
	$(CP) -p $(DIST_ETC)  $(basedir)/../$(VERNAME)/$(thisdir)/etc
	$(CP) -p $(DIST_MENU) $(basedir)/../$(VERNAME)/$(thisdir)/menu

distdepend: alldoc

# ------------------------------------------------------------------------
# DO NOT DELETE: ugly dependency list follows
