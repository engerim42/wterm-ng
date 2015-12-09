#include <WINGs/WINGs.h>
#include <WMaker.h>
/*--------------------------------*-C-*---------------------------------*
 * File:	command.c
 *----------------------------------------------------------------------*
 * Copyright (C) 1997,1998 Oezguer Kesim <kesim@math.fu-berlin.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *----------------------------------------------------------------------*/
/*----------------------------------------------------------------------* 
 * Originally written:
 *    1992      John Bovey, University of Kent at Canterbury <jdb@ukc.ac.uk>
 * Modifications:
 *    1994      Robert Nation <nation@rocket.sanders.lockheed.com>
 *              - extensive modifications
 *    1995      Garrett D'Amore <garrett@netcom.com>
 *              - vt100 printing
 *    1995      Steven Hirsch <hirsch@emba.uvm.edu>
 *              - X11 mouse report mode and support for DEC "private mode"
 *                save/restore functions.
 *    1995      Jakub Jelinek <jj@gnu.ai.mit.edu>
 *              - key-related changes to handle Shift+function keys properly.
 *    1997      MJ Olesen <olesen@me.queensu.ca>
 *              - extensive modifications
 *    1997      Raul Garcia Garcia <rgg@tid.es>
 *              - modification and cleanups for Solaris 2.x and Linux 1.2.x
 *    1997,1998 Oezguer Kesim <kesim@math.fu-berlin.de>
 *    1998      Geoff Wing <gcw@pobox.com>
 *    1998	Alfredo K. Kojima <kojima@windowmaker.org>
 *----------------------------------------------------------------------*/

#ifndef lint
static const char rcsid[] = "$Id: command.c,v 1.20 1998/09/24 18:22:36 mason Exp $";
#endif

/*{{{ includes: */
#include "wterm.h"		/* NECESSARY */

#ifdef OFFIX_DND
# include <X11/Xatom.h>
# define DndFile	2
# define DndDir		5
# define DndLink	7
#endif

#include <X11/keysym.h>
#ifndef NO_XLOCALE
# if (XtSpecificationRelease < 6)
#  define NO_XLOCALE
# else
#  define X_LOCALE
#  include <X11/Xlocale.h>
# endif
#endif				/* NO_XLOCALE */

#ifdef TTY_GID_SUPPORT
# include <grp.h>
#endif

#if defined (__svr4__)
# include <sys/resource.h>	/* for struct rlimit */
# include <sys/stropts.h>	/* for I_PUSH */
# define _NEW_TTY_CTRL		/* to get proper defines in <termios.h> */
#endif

/*}}} */

/* #define DEBUG_TTYMODE */
/* #define DEBUG_CMD */

/*{{{ terminal mode defines: */
/* use the fastest baud-rate */
#ifdef B38400
# define BAUDRATE	B38400
#else
# ifdef B19200
#  define BAUDRATE	B19200
# else
#  define BAUDRATE	B9600
# endif
#endif

/* Disable special character functions */
#ifdef _POSIX_VDISABLE
# define VDISABLE	_POSIX_VDISABLE
#else
# define VDISABLE	255
#endif

/*----------------------------------------------------------------------*
 * system default characters if defined and reasonable
 */
#ifndef CINTR
# define CINTR		'\003'	/* ^C */
#endif
#ifndef CQUIT
# define CQUIT		'\034'	/* ^\ */
#endif
#ifndef CERASE
# ifdef linux
#  define CERASE	'\177'	/* ^? */
# else
#  define CERASE	'\010'	/* ^H */
# endif
#endif
#ifndef CKILL
# define CKILL		'\025'	/* ^U */
#endif
#ifndef CEOF
# define CEOF		'\004'	/* ^D */
#endif
#ifndef CSTART
# define CSTART		'\021'	/* ^Q */
#endif
#ifndef CSTOP
# define CSTOP		'\023'	/* ^S */
#endif
#ifndef CSUSP
# define CSUSP		'\032'	/* ^Z */
#endif
#ifndef CDSUSP
# define CDSUSP		'\031'	/* ^Y */
#endif
#ifndef CRPRNT
# define CRPRNT		'\022'	/* ^R */
#endif
#ifndef CFLUSH
# define CFLUSH		'\017'	/* ^O */
#endif
#ifndef CWERASE
# define CWERASE	'\027'	/* ^W */
#endif
#ifndef CLNEXT
# define CLNEXT		'\026'	/* ^V */
#endif

#ifndef VDISCRD
# ifdef VDISCARD
#  define VDISCRD	VDISCARD
# endif
#endif

#ifndef VWERSE
# ifdef VWERASE
#  define VWERSE	VWERASE
# endif
#endif
/*}}} */

/*{{{ defines: */

#define KBUFSZ		8	/* size of keyboard mapping buffer */
#define STRING_MAX	512	/* max string size for process_xterm_seq() */
#define ESC_ARGS	32	/* max # of args for esc sequences */

/* a large REFRESH_PERIOD causes problems with `cat' */
#define REFRESH_PERIOD		1

#ifndef REFRESH_PERIOD
# define REFRESH_PERIOD		10
#endif

#ifndef MULTICLICK_TIME
# define MULTICLICK_TIME	500
#endif
#ifndef SCROLLBAR_INITIAL_DELAY
# ifdef NEXT_SCROLLER
#  define SCROLLBAR_INITIAL_DELAY	20
# else
#  define SCROLLBAR_INITIAL_DELAY	40
# endif
#endif
#ifndef SCROLLBAR_CONTINUOUS_DELAY
# define SCROLLBAR_CONTINUOUS_DELAY	2
#endif

/* time factor to slow down a `jumpy' mouse */
#define MOUSE_THRESHOLD		50
#define CONSOLE		"/dev/console"	/* console device */

/*
 * key-strings: if only these keys were standardized <sigh>
 */
#ifdef LINUX_KEYS
# define KS_HOME	"\033[1~"	/* Home == Find */
# define KS_END		"\033[4~"	/* End == Select */
#else
# define KS_HOME	"\033[7~"	/* Home */
# define KS_END		"\033[8~"	/* End */
#endif

/*
 * ESC-Z processing:
 *
 * By stealing a sequence to which other xterms respond, and sending the
 * same number of characters, but having a distinguishable sequence,
 * we can avoid having a timeout (when not under an wterm) for every login
 * shell to auto-set its DISPLAY.
 *
 * This particular sequence is even explicitly stated as obsolete since
 * about 1985, so only very old software is likely to be confused, a
 * confusion which can likely be remedied through termcap or TERM. Frankly,
 * I doubt anyone will even notice.  We provide a #ifdef just in case they
 * don't care about auto-display setting.  Just in case the ancient
 * software in question is broken enough to be case insensitive to the 'c'
 * character in the answerback string, we make the distinguishing
 * characteristic be capitalization of that character. The length of the
 * two strings should be the same so that identical read(2) calls may be
 * used.
 */
#define VT100_ANS	"\033[?1;2c"	/* vt100 answerback */
#ifndef ESCZ_ANSWER
# define ESCZ_ANSWER	VT100_ANS	/* obsolete ANSI ESC[c */
#endif
/*}}} */

/*{{{ local variables */
static char    *ptydev = NULL, *ttydev = NULL;	/* pty/tty name */
static int      cmd_fd = -1;	/* file descriptor connected to the command */
static pid_t    cmd_pid = -1;	/* process id if child */
static int      Xfd = -1;	/* file descriptor of X server connection */
static unsigned int num_fds = 0;	/* number of file descriptors being used */
static struct stat ttyfd_stat;	/* original status of the tty we will use */

#ifndef NO_SCROLLBAR_BUTTON_CONTINUAL_SCROLLING
static int      scroll_arrow_delay;
#endif

#ifdef META8_OPTION
static unsigned char meta_char = 033;	/* Alt-key prefix */
#endif
static unsigned int ModXMask = Mod1Mask;

/* DEC private modes */
#define PrivMode_132		(1LU<<0)
#define PrivMode_132OK		(1LU<<1)
#define PrivMode_rVideo		(1LU<<2)
#define PrivMode_relOrigin	(1LU<<3)
#define PrivMode_Screen		(1LU<<4)
#define PrivMode_Autowrap	(1LU<<5)
#define PrivMode_aplCUR		(1LU<<6)
#define PrivMode_aplKP		(1LU<<7)
#define PrivMode_HaveBackSpace	(1LU<<8)
#define PrivMode_BackSpace	(1LU<<9)
#define PrivMode_ShiftKeys	(1LU<<10)
#define PrivMode_VisibleCursor	(1LU<<11)
#define PrivMode_MouseX10	(1LU<<12)
#define PrivMode_MouseX11	(1LU<<13)
#define PrivMode_scrollBar	(1LU<<14)
#define PrivMode_menuBar	(1LU<<15)
#define PrivMode_TtyOutputInh	(1LU<<16)
#define PrivMode_Keypress	(1LU<<17)
/* too annoying to implement X11 highlight tracking */
/* #define PrivMode_MouseX11Track       (1LU<<18) */

#define PrivMode_mouse_report	(PrivMode_MouseX10|PrivMode_MouseX11)
#define PrivMode(test,bit)	do {					\
    if (test) PrivateModes |= (bit); else PrivateModes &= ~(bit);} while (0)

#define PrivMode_Default						 \
(PrivMode_Autowrap|PrivMode_aplKP|PrivMode_ShiftKeys|PrivMode_VisibleCursor)

static unsigned long PrivateModes = PrivMode_Default;
static unsigned long SavedModes = PrivMode_Default;

#undef PrivMode_Default

static int      refresh_count = 0, refresh_limit = 1,
		refresh_type = SLOW_REFRESH;

static Atom     wmDeleteWindow;

/* OffiX Dnd (drag 'n' drop) support */
#ifdef OFFIX_DND
static Atom     DndProtocol, DndSelection;
#endif				/* OFFIX_DND */
WMMenu *menu;
WMAppContext *app;


#ifndef NO_XLOCALE
static char    *rs_inputMethod = "";	/* XtNinputMethod */
static char    *rs_preeditType = NULL;	/* XtNpreeditType */
static XIC      Input_Context;	/* input context */
#endif				/* NO_XLOCALE */

/* command input buffering */
#ifndef BUFSIZ
# define BUFSIZ		4096
#endif
static unsigned char cmdbuf_base[BUFSIZ], *cmdbuf_ptr, *cmdbuf_endp;

/*}}} */

/*----------------------------------------------------------------------*/
/*}}} */

/*{{{ substitute system functions */
#ifndef _POSIX_VERSION
# if defined (__svr4__)
/* PROTO */
int
getdtablesize(void)
{
    struct rlimit   rlim;

    getrlimit(RLIMIT_NOFILE, &rlim);
    return rlim.rlim_cur;
}
# endif
#endif
/*}}} */

/*{{{ take care of suid/sgid super-user (root) privileges */
/* PROTO */
void
privileges(int mode)
{
#ifdef HAVE_SETEUID
    static uid_t    euid;
    static gid_t    egid;

    switch (mode) {
    case IGNORE:
    /*
     * change effective uid/gid - not real uid/gid - so we can switch
     * back to root later, as required
     */
	seteuid(getuid());
	setegid(getgid());
	break;

    case SAVE:
	euid = geteuid();
	egid = getegid();
	break;

    case RESTORE:
	seteuid(euid);
	setegid(egid);
	break;
    }
#else
# error Warning, no seteuid/setegid avaliable
# ifndef __CYGWIN32__
    switch (mode) {
    case IGNORE:
	setuid(getuid());
	setgid(getgid());
	break;

    case SAVE:
	break;
    case RESTORE:
	break;
    }
# endif
#endif
}
/*}}} */

/*{{{ signal handling, exit handler */
/*
 * Catch a SIGCHLD signal and exit if the direct child has died
 */
/* ARGSUSED */
/* PROTO */
RETSIGTYPE
Child_signal(int unused)
{
    int             pid, save_errno = errno;

    do {
	errno = 0;
    }
    while ((-1 == (pid = waitpid(-1, NULL, WNOHANG))) &&
	   (errno == EINTR));

    if (pid == cmd_pid)
	exit(EXIT_SUCCESS);
    errno = save_errno;

    signal(SIGCHLD, Child_signal);
}

/*
 * Catch a fatal signal and tidy up before quitting
 */
/* PROTO */
RETSIGTYPE
Exit_signal(int sig)
{
#ifdef DEBUG_CMD
    print_error("signal %d", sig);
#endif
    signal(sig, SIG_DFL);

#ifdef UTMP_SUPPORT
    privileges(RESTORE);
    cleanutent();
    privileges(IGNORE);
#endif

    kill(getpid(), sig);
}

/*
 * Exit gracefully, clearing the utmp entry and restoring tty attributes
 * TODO: this should free up any known resources if we can
 */
/* PROTO */
void
clean_exit(void)
{
#ifdef DEBUG_CMD
    fprintf(stderr, "Restoring \"%s\" to mode %03o, uid %d, gid %d\n",
	    ttydev, ttyfd_stat.st_mode, ttyfd_stat.st_uid, ttyfd_stat.st_gid);
#endif
    scr_release();
    privileges(RESTORE);
#ifndef __CYGWIN32__
    chmod(ttydev, ttyfd_stat.st_mode);
    chown(ttydev, ttyfd_stat.st_uid, ttyfd_stat.st_gid);
#endif
#ifdef UTMP_SUPPORT
    cleanutent();
#endif
    privileges(IGNORE);
}

/*}}} */

/*{{{ Acquire a pseudo-teletype from the system. */
/*
 * On failure, returns -1.
 * On success, returns the file descriptor.
 *
 * If successful, ttydev and ptydev point to the names of the
 * master and slave parts
 */
/* PROTO */
int
get_pty(void)
{
    int             fd = -1;

#if defined (__sgi)
    ptydev = ttydev = _getpty(&fd, O_RDWR | O_NDELAY, 0622, 0);
    if (ptydev == NULL)
	goto Failed;
#elif defined (__svr4__) || defined(__CYGWIN32__)
    {
	extern char    *ptsname();

    /* open the STREAMS, clone device /dev/ptmx (master pty) */
	if ((fd = open("/dev/ptmx", O_RDWR)) < 0) {
	    goto Failed;
	} else {
	    grantpt(fd);	/* change slave permissions */
	    unlockpt(fd);	/* unlock slave */
	    ptydev = ttydev = ptsname(fd);	/* get slave's name */
	}
    }
#elif defined (_AIX)
    if ((fd = open("/dev/ptc", O_RDWR)) < 0)
	goto Failed;
    else
	ptydev = ttydev = ttyname(fd);
#elif defined(ALL_NUMERIC_PTYS)	/* SCO OSr5 */
    static char     pty_name[] = "/dev/ptyp??\0\0\0";
    static char     tty_name[] = "/dev/ttyp??\0\0\0";
    int             len = strlen(tty_name);
    char           *c1, *c2;
    int             idx;

    ptydev = pty_name;
    ttydev = tty_name;

    for (idx = 0; idx < 256; idx++) {
	sprintf(ptydev, "%s%d", "/dev/ptyp", idx);
	sprintf(ttydev, "%s%d", "/dev/ttyp", idx);

	if (access(ttydev, F_OK) < 0) {
	    idx = 256;
	    break;
	}
	if ((fd = open(ptydev, O_RDWR)) >= 0) {
	    if (access(ttydev, R_OK | W_OK) == 0)
		goto Found;
	    close(fd);
	}
    }
    goto Failed;
#else
    static char     pty_name[] = "/dev/pty??";
    static char     tty_name[] = "/dev/tty??";
    int             len = strlen(tty_name);
    char           *c1, *c2;
    int            s;

    ttydev = malloc(30);

    if (openpty(&fd, &s, ttydev, NULL, NULL))
      return -1;

    goto Found;

    ptydev = pty_name;
    ttydev = tty_name;

# define PTYCHAR1	"pqrstuvwxyz"
# define PTYCHAR2	"0123456789abcdef"
    for (c1 = PTYCHAR1; *c1; c1++) {
	ptydev[len - 2] = ttydev[len - 2] = *c1;
	for (c2 = PTYCHAR2; *c2; c2++) {
	    ptydev[len - 1] = ttydev[len - 1] = *c2;
	    if ((fd = open(ptydev, O_RDWR)) >= 0) {
		if (access(ttydev, R_OK | W_OK) == 0)
		    goto Found;
		close(fd);
	    }
	}
    }
    goto Failed;
#endif

  Found:
    fcntl(fd, F_SETFL, O_NDELAY);
    return fd;

  Failed:
    print_error("can't open pseudo-tty");
    return -1;
}
/*}}} */

/*{{{ abort function */
/* PROTO */
void
wAbort(void)
{
    exit(1);
}
/*}}} */

/*{{{ establish a controlling teletype for new session */
/*
 * On some systems this can be done with ioctl() but on others we
 * need to re-open the slave tty.
 */
/* PROTO */
int
get_tty(void)
{
    int             fd;
    pid_t           pid;

/*
 * setsid() [or setpgrp] must be before open of the terminal,
 * otherwise there is no controlling terminal (Solaris 2.4, HP-UX 9)
 */
#ifndef ultrix
# ifdef NO_SETSID
    pid = setpgrp(0, 0);
# else
    pid = setsid();
# endif
    if (pid < 0)
	perror(rs_name);
# ifdef DEBUG_TTYMODE
    print_error("(%s: line %d): PID = %d\n", __FILE__, __LINE__, pid);
# endif
#endif				/* ultrix */

    if ((fd = open(ttydev, O_RDWR)) < 0) {
	print_error("can't open slave tty %s", ttydev);
	exit(EXIT_FAILURE);
    }
#if defined (__svr4__)
/*
 * Push STREAMS modules:
 *    ptem: pseudo-terminal hardware emulation module.
 *    ldterm: standard terminal line discipline.
 *    ttcompat: V7, 4BSD and XENIX STREAMS compatibility module.
 */
    ioctl(fd, I_PUSH, "ptem");
    ioctl(fd, I_PUSH, "ldterm");
    ioctl(fd, I_PUSH, "ttcompat");
#else				/* __svr4__ */
    {
    /* change ownership of tty to real uid and real group */
	unsigned int    mode = 0622;
	gid_t           gid = getgid();

# ifdef TTY_GID_SUPPORT
	{
	    struct group   *gr = getgrnam("tty");

	    if (gr) {
	    /* change ownership of tty to real uid, "tty" gid */
		gid = gr->gr_gid;
		mode = 0620;
	    }
	}
# endif				/* TTY_GID_SUPPORT */
	privileges(RESTORE);
# ifndef __CYGWIN32__
	fchown(fd, getuid(), gid);	/* fail silently */
	fchmod(fd, mode);
# endif
	privileges(IGNORE);
    }
#endif				/* __svr4__ */

/*
 * Close all file descriptors.  If only stdin/out/err are closed,
 * child processes remain alive upon deletion of the window.
 */
    {
	int             i;

	for (i = 0; i < num_fds; i++)
	    if (i != fd)
		close(i);
    }

/* Reopen stdin, stdout and stderr over the tty file descriptor */
    dup(fd);			/* 0: stdin */
    dup(fd);			/* 1: stdout */
    dup(fd);			/* 2: stderr */

    if (fd > 2)
	close(fd);

#ifdef ultrix
    if ((fd = open("/dev/tty", O_RDONLY)) >= 0) {
	ioctl(fd, TIOCNOTTY, 0);
	close(fd);
    } else {
	pid = setpgrp(0, 0);
	if (pid < 0)
	    perror(rs_name);
    }

/* no error, we could run with no tty to begin with */
#else				/* ultrix */
# ifdef TIOCSCTTY
    ioctl(0, TIOCSCTTY, 0);
# endif

/* set process group */
# if defined (_POSIX_VERSION) || defined (__svr4__)
    tcsetpgrp(0, pid);
# elif defined (TIOCSPGRP)
    ioctl(0, TIOCSPGRP, &pid);
# endif

/* svr4 problems: reports no tty, no job control */
/* # if !defined (__svr4__) && defined (TIOCSPGRP) */

    close(open(ttydev, O_RDWR, 0));
/* # endif */
#endif				/* ultrix */

    privileges(IGNORE);

    return fd;
}
/*}}} */

/*{{{ debug_ttymode() */
#ifdef DEBUG_TTYMODE
/* PROTO */
void
debug_ttymode(ttymode_t * ttymode)
{
# ifdef HAVE_TERMIOS_H
/* c_iflag bits */
    fprintf(stderr, "Input flags\n");

/* cpp token stringize doesn't work on all machines <sigh> */
#  define FOO(flag,name)		\
    if ((ttymode->c_iflag) & flag)	\
	fprintf (stderr, "%s ", name)

/* c_iflag bits */
    FOO(IGNBRK, "IGNBRK");
    FOO(BRKINT, "BRKINT");
    FOO(IGNPAR, "IGNPAR");
    FOO(PARMRK, "PARMRK");
    FOO(INPCK, "INPCK");
    FOO(ISTRIP, "ISTRIP");
    FOO(INLCR, "INLCR");
    FOO(IGNCR, "IGNCR");
    FOO(ICRNL, "ICRNL");
    FOO(IXON, "IXON");
    FOO(IXOFF, "IXOFF");
#  ifdef IUCLC
    FOO(IUCLC, "IUCLC");
#  endif
#  ifdef IXANY
    FOO(IXANY, "IXANY");
#  endif
#  ifdef IMAXBEL
    FOO(IMAXBEL, "IMAXBEL");
#  endif
    fprintf(stderr, "\n\n");

#  undef FOO
#  define FOO(entry, name) \
    fprintf (stderr, "%s = %#3o\n", name, ttymode->c_cc [entry])

    FOO(VINTR, "VINTR");
    FOO(VQUIT, "VQUIT");
    FOO(VERASE, "VERASE");
    FOO(VKILL, "VKILL");
    FOO(VEOF, "VEOF");
    FOO(VEOL, "VEOL");
#  ifdef VEOL2
    FOO(VEOL2, "VEOL2");
#  endif
#  ifdef VSWTC
    FOO(VSWTC, "VSWTC");
#  endif
#  ifdef VSWTCH
    FOO(VSWTCH, "VSWTCH");
#  endif
    FOO(VSTART, "VSTART");
    FOO(VSTOP, "VSTOP");
    FOO(VSUSP, "VSUSP");
#  ifdef VDSUSP
    FOO(VDSUSP, "VDSUSP");
#  endif
#  ifdef VREPRINT
    FOO(VREPRINT, "VREPRINT");
#  endif
#  ifdef VDISCRD
    FOO(VDISCRD, "VDISCRD");
#  endif
#  ifdef VWERSE
    FOO(VWERSE, "VWERSE");
#  endif
#  ifdef VLNEXT
    FOO(VLNEXT, "VLNEXT");
#  endif
    fprintf(stderr, "\n\n");
#  undef FOO
# endif				/* HAVE_TERMIOS_H */
}
#endif				/* DEBUG_TTYMODE */
/*}}} */

/*{{{ get_ttymode() */
/* PROTO */
void
get_ttymode(ttymode_t * tio)
{
#ifdef HAVE_TERMIOS_H
/*
 * standard System V termios interface
 */
    if (GET_TERMIOS(0, tio) < 0) {
    /* return error - use system defaults */
	tio->c_cc[VINTR] = CINTR;
	tio->c_cc[VQUIT] = CQUIT;
	tio->c_cc[VERASE] = CERASE;
	tio->c_cc[VKILL] = CKILL;
	tio->c_cc[VSTART] = CSTART;
	tio->c_cc[VSTOP] = CSTOP;
	tio->c_cc[VSUSP] = CSUSP;
# ifdef VDSUSP
	tio->c_cc[VDSUSP] = CDSUSP;
# endif
# ifdef VREPRINT
	tio->c_cc[VREPRINT] = CRPRNT;
# endif
# ifdef VDISCRD
	tio->c_cc[VDISCRD] = CFLUSH;
# endif
# ifdef VWERSE
	tio->c_cc[VWERSE] = CWERASE;
# endif
# ifdef VLNEXT
	tio->c_cc[VLNEXT] = CLNEXT;
# endif
    }
    tio->c_cc[VEOF] = CEOF;
    tio->c_cc[VEOL] = VDISABLE;
# ifdef VEOL2
    tio->c_cc[VEOL2] = VDISABLE;
# endif
# ifdef VSWTC
    tio->c_cc[VSWTC] = VDISABLE;
# endif
# ifdef VSWTCH
    tio->c_cc[VSWTCH] = VDISABLE;
# endif
# if VMIN != VEOF
    tio->c_cc[VMIN] = 1;
# endif
# if VTIME != VEOL
    tio->c_cc[VTIME] = 0;
# endif

/* input modes */
    tio->c_iflag = (BRKINT | IGNPAR | ICRNL | IXON
# ifdef IMAXBEL
		    | IMAXBEL
# endif
	);

/* output modes */
    tio->c_oflag = (OPOST | ONLCR);

/* control modes */
    tio->c_cflag = (CS8 | CREAD);

/* line discipline modes */
    tio->c_lflag = (ISIG | ICANON | IEXTEN | ECHO | ECHOE | ECHOK
# if defined (ECHOCTL) && defined (ECHOKE)
		    | ECHOCTL | ECHOKE
# endif
	);

#if 0
/*
 * guess an appropriate value for Backspace
 */
# ifdef FORCE_BACKSPACE
    PrivMode(1, PrivMode_BackSpace);
# elif defined (FORCE_DELETE)
    PrivMode(0, PrivMode_BackSpace);
# else
    PrivMode((tio->c_cc[VERASE] == '\b'), PrivMode_BackSpace);
# endif
#endif				/* if 0 */

#else				/* HAVE_TERMIOS_H */

/*
 * sgtty interface
 */

/* get parameters -- gtty */
    if (ioctl(0, TIOCGETP, &(tio->sg)) < 0) {
	tio->sg.sg_erase = CERASE;	/* ^H */
	tio->sg.sg_kill = CKILL;	/* ^U */
    }
    tio->sg.sg_flags = (CRMOD | ECHO | EVENP | ODDP);

/* get special characters */
    if (ioctl(0, TIOCGETC, &(tio->tc)) < 0) {
	tio->tc.t_intrc = CINTR;	/* ^C */
	tio->tc.t_quitc = CQUIT;	/* ^\ */
	tio->tc.t_startc = CSTART;	/* ^Q */
	tio->tc.t_stopc = CSTOP;	/* ^S */
	tio->tc.t_eofc = CEOF;	/* ^D */
	tio->tc.t_brkc = -1;
    }
/* get local special chars */
    if (ioctl(0, TIOCGLTC, &(tio->lc)) < 0) {
	tio->lc.t_suspc = CSUSP;	/* ^Z */
	tio->lc.t_dsuspc = CDSUSP;	/* ^Y */
	tio->lc.t_rprntc = CRPRNT;	/* ^R */
	tio->lc.t_flushc = CFLUSH;	/* ^O */
	tio->lc.t_werasc = CWERASE;	/* ^W */
	tio->lc.t_lnextc = CLNEXT;	/* ^V */
    }
/* get line discipline */
    ioctl(0, TIOCGETD, &(tio->line));
# ifdef NTTYDISC
    tio->line = NTTYDISC;
# endif				/* NTTYDISC */
    tio->local = (LCRTBS | LCRTERA | LCTLECH | LPASS8 | LCRTKIL);

/*
 * guess an appropriate value for Backspace
 */
#if 0
# ifdef FORCE_BACKSPACE
    PrivMode(1, PrivMode_BackSpace);
# elif defined (FORCE_DELETE)
    PrivMode(0, PrivMode_BackSpace);
# else
    PrivMode((tio->sg.sg_erase == '\b'), PrivMode_BackSpace);
# endif
#endif				/* if 0 */

#endif				/* HAVE_TERMIOS_H */
}
/*}}} */

/*{{{ run_command() */
/*
 * Run the command in a subprocess and return a file descriptor for the
 * master end of the pseudo-teletype pair with the command talking to
 * the slave.
 */
/* PROTO */
int
run_command(char *argv[])
{
    ttymode_t       tio;
    int             ptyfd;

    ptyfd = get_pty();
    if (ptyfd < 0)
	return -1;

/* store original tty status for restoration clean_exit() -- rgg 04/12/95 */
    lstat(ttydev, &ttyfd_stat);
#ifdef DEBUG_CMD
    fprintf(stderr, "Original settings of %s are mode %o, uid %d, gid %d\n",
	    ttydev, ttyfd_stat.st_mode, ttyfd_stat.st_uid, ttyfd_stat.st_gid);
#endif

/* install exit handler for cleanup */
#ifdef HAVE_ATEXIT
    atexit(clean_exit);
#else
# if defined (__sun__)
    on_exit(clean_exit, NULL);	/* non-ANSI exit handler */
# else
#  ifdef UTMP_SUPPORT
    print_error("no atexit(), UTMP entries can't be cleaned");
#  endif
# endif
#endif

/*
 * get tty settings before fork()
 * and make a reasonable guess at the value for BackSpace
 */
    get_ttymode(&tio);
#if 0
/* add Backspace value */
    SavedModes |= (PrivateModes & PrivMode_BackSpace);
#endif

/* add value for scrollBar */
    if (scrollbar_visible()) {
	PrivateModes |= PrivMode_scrollBar;
	SavedModes |= PrivMode_scrollBar;
    }
    if (menubar_visible()) {
	PrivateModes |= PrivMode_menuBar;
	SavedModes |= PrivMode_menuBar;
    }
#ifdef DEBUG_TTYMODE
    debug_ttymode(&tio);
#endif

/* spin off the command interpreter */
    signal(SIGHUP, Exit_signal);
#ifndef __svr4__
    signal(SIGINT, Exit_signal);
#endif
    signal(SIGQUIT, Exit_signal);
    signal(SIGTERM, Exit_signal);
    signal(SIGCHLD, Child_signal);

/* need to trap SIGURG for SVR4 (Unixware) rlogin */
/* signal (SIGURG, SIG_DFL); */

    cmd_pid = fork();
    if (cmd_pid < 0) {
	print_error("can't fork");
	return -1;
    }
    if (cmd_pid == 0) {		/* child */
    /* signal (SIGHUP, Exit_signal); */
    /* signal (SIGINT, Exit_signal); */
#ifdef HAVE_UNSETENV
    /* avoid passing old settings and confusing term size */
	unsetenv("LINES");
	unsetenv("COLUMNS");
    /* avoid passing termcap since terminfo should be okay */
	unsetenv("TERMCAP");
#endif				/* HAVE_UNSETENV */
    /* establish a controlling teletype for the new session */
	get_tty();

    /* initialize terminal attributes */
	SET_TTYMODE(0, &tio);

    /* become virtual console, fail silently */
	if (Options & Opt_console) {
#ifdef TIOCCONS
	    unsigned int    on = 1;

	    ioctl(0, TIOCCONS, &on);
#elif defined (SRIOCSREDIR)
	    int             fd = open(CONSOLE, O_WRONLY);

	    if (fd < 0 || ioctl(fd, SRIOCSREDIR, 0) < 0) {
		if (fd >= 0)
		    close(fd);
	    }
#endif				/* SRIOCSREDIR */
	}
	tt_winsize(0);		/* set window size */

    /* reset signals and spin off the command interpreter */
	signal(SIGINT, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGCHLD, SIG_DFL);
    /*
     * mimick login's behavior by disabling the job control signals
     * a shell that wants them can turn them back on
     */
#ifdef SIGTSTP
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
#endif				/* SIGTSTP */

    /* command interpreter path */
	if (argv != NULL) {
#ifdef DEBUG_CMD
	    int             i;

	    for (i = 0; argv[i]; i++)
		fprintf(stderr, "argv [%d] = \"%s\"\n", i, argv[i]);
#endif
	    execvp(argv[0], argv);
	    print_error("can't execute \"%s\"", argv[0]);
	} else {
	    const char     *argv0, *shell;

	    if ((shell = getenv("SHELL")) == NULL || *shell == '\0')
		shell = "/bin/sh";

	    argv0 = my_basename(shell);
	    if (Options & Opt_loginShell) {
		char           *p = MALLOC((strlen(argv0) + 2) * sizeof(char));

		p[0] = '-';
		STRCPY(&p[1], argv0);
		argv0 = p;
	    }
	    execlp(shell, argv0, NULL);
	    print_error("can't execute \"%s\"", shell);
	}
	exit(EXIT_FAILURE);
    }
#ifdef UTMP_SUPPORT
    privileges(RESTORE);
    if (!(Options & Opt_utmpInhibit))
	makeutent(ttydev, display_name);	/* stamp /etc/utmp */
    privileges(IGNORE);
#endif

    return ptyfd;
}
/*}}} */

/*{{{ init_command() */
/* PROTO */
void
init_command(char *argv[])
{
/*
 * Initialize the command connection.
 * This should be called after the X server connection is established.
 */

/* Enable delete window protocol */
    wmDeleteWindow = XInternAtom(Xdisplay, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(Xdisplay, TermWin.parent, &wmDeleteWindow, 1);

#ifdef OFFIX_DND
/* Enable OffiX Dnd (drag 'n' drop) protocol */
    DndProtocol = XInternAtom(Xdisplay, "DndProtocol", False);
    DndSelection = XInternAtom(Xdisplay, "DndSelection", False);
#endif				/* OFFIX_DND */

    init_xlocale();

/* get number of available file descriptors */
#ifdef _POSIX_VERSION
    num_fds = sysconf(_SC_OPEN_MAX);
#else
    num_fds = getdtablesize();
#endif

#ifdef META8_OPTION
    meta_char = (Options & Opt_meta8 ? 0x80 : 033);
    if (rs_modifier
	&& strlen(rs_modifier) == 4
	&& toupper(*rs_modifier) == 'M'
	&& toupper(rs_modifier[1]) == 'O'
	&& toupper(rs_modifier[2]) == 'D')
	switch (rs_modifier[3]) {
	case '2':
	    ModXMask = Mod2Mask;
	    break;
	case '3':
	    ModXMask = Mod3Mask;
	    break;
	case '4':
	    ModXMask = Mod4Mask;
	    break;
	case '5':
	    ModXMask = Mod5Mask;
	    break;
	default:
	    ModXMask = Mod1Mask;
	}
#endif
    if (Options & Opt_scrollTtyOutputInh)
	PrivateModes |= PrivMode_TtyOutputInh;
    if (Options & Opt_scrollKeypress)
	PrivateModes |= PrivMode_Keypress;
#ifndef NO_BACKSPACE_KEY
    if (strcmp(rs_backspace_key, "DEC") == 0)
	PrivateModes |= PrivMode_HaveBackSpace;
#endif

#ifdef GREEK_SUPPORT
    greek_init();
#endif

    Xfd = XConnectionNumber(Xdisplay);
    cmdbuf_ptr = cmdbuf_endp = cmdbuf_base;

    if ((cmd_fd = run_command(argv)) < 0) {
	print_error("aborting");
	exit(EXIT_FAILURE);
    }
}
/*}}} */

/*{{{ Xlocale */
/*
 * This is more or less stolen straight from XFree86 xterm.
 * This should support all European type languages.
 */
/* PROTO */
void
init_xlocale(void)
{
#ifndef NO_XLOCALE
    char           *p, *s, buf[32], tmp[1024];
    XIM             xim = NULL;
    XIMStyle        input_style = 0;
    XIMStyles      *xim_styles = NULL;
    int             found;

    Input_Context = NULL;

# ifndef NO_SETLOCALE
    setlocale(LC_CTYPE, "");	/* XXX: should we do this? */
# endif
    if (rs_inputMethod == NULL || !*rs_inputMethod) {
	if ((p = XSetLocaleModifiers("@im=none")) != NULL && *p)
	    xim = XOpenIM(Xdisplay, NULL, NULL, NULL);
    } else {
	STRCPY(tmp, rs_inputMethod);
	for (s = tmp; *s; s++) {
	    char           *end, *next_s;

	    for (; *s && isspace(*s); s++)
		/* */ ;
	    if (!*s)
		break;
	    for (end = s; (*end && (*end != ',')); end++)
		/* */ ;
	    for (next_s = end--; ((end >= s) && isspace(*end)); end--)
		/* */ ;
	    *++end = '\0';
	    if (*s) {
		STRCPY(buf, "@im=");
		strcat(buf, s);
		if ((p = XSetLocaleModifiers(buf)) != NULL && *p &&
		    (xim = XOpenIM(Xdisplay, NULL, NULL, NULL)) != NULL)
		    break;
	    }
	    if (!*(s = next_s))
		break;
	}
    }

    if (xim == NULL && (p = XSetLocaleModifiers("")) != NULL && *p)
	xim = XOpenIM(Xdisplay, NULL, NULL, NULL);

    if (xim == NULL) {
	print_error("Failed to open input method");
	return;
    }
    if (XGetIMValues(xim, XNQueryInputStyle, &xim_styles, NULL) || !xim_styles) {
	print_error("input method doesn't support any style");
	XCloseIM(xim);
	return;
    }
    STRCPY(tmp, (rs_preeditType ? rs_preeditType : "Root"));
    for (found = 0, s = tmp; *s && !found; ) {
	unsigned short  i;
	char           *end, *next_s;

	for (; *s && isspace(*s); s++)
	    /* */ ;
	if (!*s)
	    break;
	for (end = s; (*end && (*end != ',')); end++)
	    /* */ ;
	for (next_s = end--; ((end >= s) && isspace(*end));)
	    *end-- = 0;

	if (!strcmp(s, "OverTheSpot"))
	    input_style = (XIMPreeditPosition | XIMStatusArea);
	else if (!strcmp(s, "OffTheSpot"))
	    input_style = (XIMPreeditArea | XIMStatusArea);
	else if (!strcmp(s, "Root"))
	    input_style = (XIMPreeditNothing | XIMStatusNothing);

	for (i = 0; i < xim_styles->count_styles; i++)
	    if (input_style == xim_styles->supported_styles[i]) {
		found = 1;
		break;
	    }
	s = next_s;
    }
    XFree(xim_styles);

    if (found == 0) {
	print_error("input method doesn't support my preedit type");
	XCloseIM(xim);
	return;
    }
/*
 * This program only understands the Root preedit_style yet
 * Then misc.preedit_type should default to:
 *          "OverTheSpot,OffTheSpot,Root"
 *  /MaF
 */
    if (input_style != (XIMPreeditNothing | XIMStatusNothing)) {
	print_error("This program only supports the \"Root\" preedit type");
	XCloseIM(xim);
	return;
    }
    Input_Context = XCreateIC(xim, XNInputStyle, input_style,
			      XNClientWindow, TermWin.parent,
			      XNFocusWindow, TermWin.parent,
			      NULL);

    if (Input_Context == NULL) {
	print_error("Failed to create input context");
	XCloseIM(xim);
    }
#endif				/* NO_XLOCALE */
}
/*}}} */

/*{{{ window resizing */
/*
 * Tell the teletype handler what size the window is.
 * Called after a window size change.
 */
/* PROTO */
void
tt_winsize(int fd)
{
    struct winsize  ws;

    if (fd < 0)
	return;

    ws.ws_col = (unsigned short)TermWin.ncol;
    ws.ws_row = (unsigned short)TermWin.nrow;
#ifndef __CYGWIN32__
    ws.ws_xpixel = ws.ws_ypixel = 0;
#endif
    ioctl(fd, TIOCSWINSZ, &ws);
}

/* PROTO */
void
tt_resize(void)
{
    tt_winsize(cmd_fd);
}

/*}}} */

/*{{{ Convert the keypress event into a string */
/* PROTO */
void
lookup_key(XEvent * ev)
{
    static int      numlock_state = 0;

#ifdef DEBUG_CMD
    static int      debug_key = 1;	/* accessible by a debugger only */
#endif
#ifdef GREEK_SUPPORT
    static short    greek_mode = 0;
#endif
    static XComposeStatus compose =
    {NULL, 0};
    static unsigned char kbuf[KBUFSZ];
    int             ctrl, meta, shft, len;
    KeySym          keysym;

/*
 * use Num_Lock to toggle Keypad on/off.  If Num_Lock is off, allow an
 * escape sequence to toggle the Keypad.
 *
 * Always permit `shift' to override the current setting
 */
    shft = (ev->xkey.state & ShiftMask);
    ctrl = (ev->xkey.state & ControlMask);
    meta = (ev->xkey.state & ModXMask);
    if (numlock_state || (ev->xkey.state & Mod5Mask)) {
	numlock_state = (ev->xkey.state & Mod5Mask);	/* numlock toggle */
	PrivMode((!numlock_state), PrivMode_aplKP);
    }
#ifndef NO_XLOCALE
    if (!XFilterEvent(ev, *(&ev->xkey.window))) {
	if (Input_Context != NULL) {
	    Status          status_return;

	    len = XmbLookupString(Input_Context, &ev->xkey, kbuf,
				  sizeof(kbuf), &keysym,
				  &status_return);
	} else {
	    len = XLookupString(&ev->xkey, kbuf,
				sizeof(kbuf), &keysym,
				&compose);
	}
    } else
	len = 0;
#else				/* NO_XLOCALE */
    len = XLookupString(&ev->xkey, (char *) kbuf, sizeof(kbuf), &keysym, &compose);
/*
 * have unmapped Latin[2-4] entries -> Latin1
 * good for installations  with correct fonts, but without XLOCAL
 */
    if (!len && (keysym >= 0x0100) && (keysym < 0x0400)) {
	len = 1;
	kbuf[0] = (keysym & 0xFF);
    }
#endif				/* NO_XLOCALE */

    if (len && (Options & Opt_scrollKeypress))
	TermWin.view_start = 0;

/* for some backwards compatibility */
#if defined (HOTKEY_CTRL) || defined (HOTKEY_META)
# ifdef HOTKEY_CTRL
#  define HOTKEY	ctrl
# else
#  ifdef HOTKEY_META
#   define HOTKEY	meta
#  endif
# endif
    if (HOTKEY) {
	if (keysym == ks_bigfont) {
	    change_font(0, FONT_UP);
	    return;
	} else if (keysym == ks_smallfont) {
	    change_font(0, FONT_DN);
	    return;
	}
    }
# undef HOTKEY
#endif

    if (shft) {
    /* Shift + F1 - F10 generates F11 - F20 */
	if (keysym >= XK_F1 && keysym <= XK_F10) {
	    keysym += (XK_F11 - XK_F1);
	    shft = 0;		/* turn off Shift */
	} else if (!ctrl && !meta && (PrivateModes & PrivMode_ShiftKeys)) {
	    int             lnsppg = TermWin.nrow * 4 / 5;

#ifdef PAGING_CONTEXT_LINES
	    lnsppg = TermWin.nrow - PAGING_CONTEXT_LINES;
#endif

	    switch (keysym) {
	    /* normal XTerm key bindings */
	    case XK_Prior:	/* Shift+Prior = scroll back */
		if (TermWin.saveLines) {
		    scr_page(UP, lnsppg);
		    return;
		}
		break;

	    case XK_Next:	/* Shift+Next = scroll forward */
		if (TermWin.saveLines) {
		    scr_page(DN, lnsppg);
		    return;
		}
		break;

	    case XK_Insert:	/* Shift+Insert = paste mouse selection */
		selection_request(ev->xkey.time, ev->xkey.x, ev->xkey.y);
		return;
		break;

	    /* wterm extras */
	    case XK_KP_Add:	/* Shift+KP_Add = bigger font */
		change_font(0, FONT_UP);
		return;
		break;

	    case XK_KP_Subtract:	/* Shift+KP_Subtract = smaller font */
		change_font(0, FONT_DN);
		return;
		break;
	    }
	}
    }
#ifdef UNSHIFTED_SCROLLKEYS
    else if (!ctrl && !meta) {
	switch (keysym) {
	case XK_Prior:
	    if (TermWin.saveLines) {
		scr_page(UP, TermWin.nrow * 4 / 5);
		return;
	    }
	    break;

	case XK_Next:
	    if (TermWin.saveLines) {
		scr_page(DN, TermWin.nrow * 4 / 5);
		return;
	    }
	    break;
	}
    }
#endif

    switch (keysym) {
    case XK_Print:
#ifdef PRINTPIPE
	scr_printscreen(ctrl | shft);
	return;
#endif
	break;

    case XK_Mode_switch:
#ifdef GREEK_SUPPORT
	greek_mode = !greek_mode;
	if (greek_mode) {
	    xterm_seq(XTerm_title, (greek_getmode() == GREEK_ELOT928 ?
				    "[Greek: iso]" : "[Greek: ibm]"));
	    greek_reset();
	} else
	    xterm_seq(XTerm_title, APL_NAME "-" VERSION);
	return;
#endif
	break;
    }

    if (keysym >= 0xFF00 && keysym <= 0xFFFF) {
#ifdef KEYSYM_RESOURCE
	if (!(shft | ctrl) && KeySym_map[keysym - 0xFF00] != NULL) {
	    const unsigned char *kbuf;
	    unsigned int    len;

	    kbuf = (KeySym_map[keysym - 0xFF00]);
	    len = *kbuf++;

	/* escape prefix */
	    if (meta
# ifdef META8_OPTION
		&& (meta_char == 033)
# endif
		) {
		const unsigned char ch = '\033';

		tt_write(&ch, 1);
	    }
	    tt_write(kbuf, len);
	    return;
	} else
#endif
	    switch (keysym) {
#ifndef NO_BACKSPACE_KEY
	    case XK_BackSpace:
		if (PrivateModes & PrivMode_HaveBackSpace) {
		    len = 1;
		    kbuf[0] = (((PrivateModes & PrivMode_BackSpace) ?
				!(shft | ctrl) : (shft | ctrl)) ? '\b' : '\177');
		} else
		    len = strlen(STRCPY(kbuf, rs_backspace_key));
		break;
#endif
#ifndef NO_DELETE_KEY
	    case XK_Delete:
		len = strlen(STRCPY(kbuf, rs_delete_key));
		break;
#endif
	    case XK_Tab:
		if (shft) {
		    len = 3;
		    STRCPY(kbuf, "\033[Z");
		}
		break;

#ifdef XK_KP_Home
	    case XK_KP_Home:
	    /* allow shift to override */
		if ((PrivateModes & PrivMode_aplKP) ? !shft : shft) {
		    len = 3;
		    STRCPY(kbuf, "\033Ow");
		    break;
		}
	    /* -> else FALL THROUGH */
#endif
	    case XK_Home:
		len = strlen(STRCPY(kbuf, KS_HOME));
		break;

#ifdef XK_KP_Left
	    case XK_KP_Left:	/* \033Ot or standard */
	    case XK_KP_Up:	/* \033Ox or standard */
	    case XK_KP_Right:	/* \033Ov or standard */
	    case XK_KP_Down:	/* \033Ow or standard */
		if ((PrivateModes && PrivMode_aplKP) ? !shft : shft) {
		    len = 3;
		    STRCPY(kbuf, "\033OZ");
		    kbuf[2] = ("txvw"[keysym - XK_KP_Left]);
		    break;
		} else {
		/* translate to std. cursor key */
		    keysym = XK_Left + (keysym - XK_KP_Left);
		}
	    /* FALL THROUGH */
#endif
	    case XK_Left:	/* "\033[D" */
	    case XK_Up:	/* "\033[A" */
	    case XK_Right:	/* "\033[C" */
	    case XK_Down:	/* "\033[B" */
		len = 3;
		STRCPY(kbuf, "\033[@");
		kbuf[2] = ("DACB"[keysym - XK_Left]);
	    /* do Shift first */
		if (shft) {
		    kbuf[2] = ("dacb"[keysym - XK_Left]);
		} else if (ctrl) {
		    kbuf[1] = 'O';
		    kbuf[2] = ("dacb"[keysym - XK_Left]);
		} else if (PrivateModes & PrivMode_aplCUR) {
		    kbuf[1] = 'O';
		}
		break;

#ifndef UNSHIFTED_SCROLLKEYS
# ifdef XK_KP_Prior
	    case XK_KP_Prior:
	    /* allow shift to override */
		if ((PrivateModes & PrivMode_aplKP) ? !shft : shft) {
		    len = 3;
		    STRCPY(kbuf, "\033Oy");
		    break;
		}
	    /* -> else FALL THROUGH */
# endif
	    case XK_Prior:
		len = 4;
		STRCPY(kbuf, "\033[5~");
		break;
# ifdef XK_KP_Next
	    case XK_KP_Next:
	    /* allow shift to override */
		if ((PrivateModes & PrivMode_aplKP) ? !shft : shft) {
		    len = 3;
		    STRCPY(kbuf, "\033Os");
		    break;
		}
	    /* -> else FALL THROUGH */
# endif
	    case XK_Next:
		len = 4;
		STRCPY(kbuf, "\033[6~");
		break;
#endif
#ifdef XK_KP_End
	    case XK_KP_End:
	    /* allow shift to override */
		if ((PrivateModes & PrivMode_aplKP) ? !shft : shft) {
		    len = 3;
		    STRCPY(kbuf, "\033Oq");
		    break;
		}
	    /* -> else FALL THROUGH */
#endif
	    case XK_End:
		len = strlen(STRCPY(kbuf, KS_END));
		break;

	    case XK_Select:
		len = 4;
		STRCPY(kbuf, "\033[4~");
		break;
#ifdef DXK_Remove		/* support for DEC remove like key */
	    case DXK_Remove:	/* drop */
#endif
	    case XK_Execute:
		len = 4;
		STRCPY(kbuf, "\033[3~");
		break;
	    case XK_Insert:
		len = 4;
		STRCPY(kbuf, "\033[2~");
		break;

	    case XK_Menu:
		len = 5;
		STRCPY(kbuf, "\033[29~");
		break;
	    case XK_Find:
		len = 4;
		STRCPY(kbuf, "\033[1~");
		break;
	    case XK_Help:
		len = 5;
		STRCPY(kbuf, "\033[28~");
		break;

	    case XK_KP_Enter:
	    /* allow shift to override */
		if ((PrivateModes & PrivMode_aplKP) ? !shft : shft) {
		    len = 3;
		    STRCPY(kbuf, "\033OM");
		} else {
		    len = 1;
		    kbuf[0] = '\r';
		}
		break;

#ifdef XK_KP_Begin
	    case XK_KP_Begin:
		len = 3;
		STRCPY(kbuf, "\033Ou");
		break;

	    case XK_KP_Insert:
		len = 3;
		STRCPY(kbuf, "\033Op");
		break;

	    case XK_KP_Delete:
		len = 3;
		STRCPY(kbuf, "\033On");
		break;
#endif

	    case XK_KP_F1:	/* "\033OP" */
	    case XK_KP_F2:	/* "\033OQ" */
	    case XK_KP_F3:	/* "\033OR" */
	    case XK_KP_F4:	/* "\033OS" */
		len = 3;
		STRCPY(kbuf, "\033OP");
		kbuf[2] += (keysym - XK_KP_F1);
		break;

	    case XK_KP_Multiply:	/* "\033Oj" : "*" */
	    case XK_KP_Add:	/* "\033Ok" : "+" */
	    case XK_KP_Separator:	/* "\033Ol" : "," */
	    case XK_KP_Subtract:	/* "\033Om" : "-" */
	    case XK_KP_Decimal:	/* "\033On" : "." */
	    case XK_KP_Divide:	/* "\033Oo" : "/" */
	    case XK_KP_0:	/* "\033Op" : "0" */
	    case XK_KP_1:	/* "\033Oq" : "1" */
	    case XK_KP_2:	/* "\033Or" : "2" */
	    case XK_KP_3:	/* "\033Os" : "3" */
	    case XK_KP_4:	/* "\033Ot" : "4" */
	    case XK_KP_5:	/* "\033Ou" : "5" */
	    case XK_KP_6:	/* "\033Ov" : "6" */
	    case XK_KP_7:	/* "\033Ow" : "7" */
	    case XK_KP_8:	/* "\033Ox" : "8" */
	    case XK_KP_9:	/* "\033Oy" : "9" */
	    /* allow shift to override */
		if ((PrivateModes & PrivMode_aplKP) ? !shft : shft) {
		    len = 3;
		    STRCPY(kbuf, "\033Oj");
		    kbuf[2] += (keysym - XK_KP_Multiply);
		} else {
		    len = 1;
		    kbuf[0] = ('*' + (keysym - XK_KP_Multiply));
		}
		break;

#define FKEY(n, fkey)							\
    len = 5;								\
    sprintf((char *) kbuf,"\033[%02d~", (int)((n) + (keysym - fkey)))

	    case XK_F1:		/* "\033[11~" */ 
	    case XK_F2:		/* "\033[12~" */
	    case XK_F3:		/* "\033[13~" */
	    case XK_F4:		/* "\033[14~" */
	    case XK_F5:		/* "\033[15~" */
		FKEY(11, XK_F1);
		break;

	    case XK_F6:		/* "\033[17~" */
	    case XK_F7:		/* "\033[18~" */
	    case XK_F8:		/* "\033[19~" */
	    case XK_F9:		/* "\033[20~" */
	    case XK_F10:	/* "\033[21~" */
		FKEY(17, XK_F6);
		break;

	    case XK_F11:	/* "\033[23~" */
	    case XK_F12:	/* "\033[24~" */
	    case XK_F13:	/* "\033[25~" */
	    case XK_F14:	/* "\033[26~" */
		FKEY(23, XK_F11);
		break;

	    case XK_F15:	/* "\033[28~" */
	    case XK_F16:	/* "\033[29~" */
		FKEY(28, XK_F15);
		break;

	    case XK_F17:	/* "\033[31~" */
	    case XK_F18:	/* "\033[32~" */
	    case XK_F19:	/* "\033[33~" */
	    case XK_F20:	/* "\033[34~" */
	    case XK_F21:	/* "\033[35~" */
	    case XK_F22:	/* "\033[36~" */
	    case XK_F23:	/* "\033[37~" */
	    case XK_F24:	/* "\033[38~" */
	    case XK_F25:	/* "\033[39~" */
	    case XK_F26:	/* "\033[40~" */
	    case XK_F27:	/* "\033[41~" */
	    case XK_F28:	/* "\033[42~" */
	    case XK_F29:	/* "\033[43~" */
	    case XK_F30:	/* "\033[44~" */
	    case XK_F31:	/* "\033[45~" */
	    case XK_F32:	/* "\033[46~" */
	    case XK_F33:	/* "\033[47~" */
	    case XK_F34:	/* "\033[48~" */
	    case XK_F35:	/* "\033[49~" */
		FKEY(31, XK_F17);
		break;
#undef FKEY
	    }
    /*
     * Pass meta for all function keys, if 'meta' option set
     */
#ifdef META8_OPTION
	if (meta && (meta_char == 0x80) && len > 0) {
	    kbuf[len - 1] |= 0x80;
	}
#endif
    } else if (ctrl && keysym == XK_minus) {
	len = 1;
	kbuf[0] = '\037';	/* Ctrl-Minus generates ^_ (31) */
    } else {
#ifdef META8_OPTION
    /* set 8-bit on */
	if (meta && (meta_char == 0x80)) {
	    unsigned char  *ch;

	    for (ch = kbuf; ch < kbuf + len; ch++)
		*ch |= 0x80;
	    meta = 0;
	}
#endif
#ifdef GREEK_SUPPORT
	if (greek_mode)
	    len = greek_xlat(kbuf, len);
#endif
    /* nil */ ;
    }

    if (len <= 0)
	return;			/* not mapped */

/*
 * these modifications only affect the static keybuffer
 * pass Shift/Control indicators for function keys ending with `~'
 *
 * eg,
 *   Prior = "ESC[5~"
 *   Shift+Prior = "ESC[5~"
 *   Ctrl+Prior = "ESC[5^"
 *   Ctrl+Shift+Prior = "ESC[5@"
 * Meta adds an Escape prefix (with META8_OPTION, if meta == <escape>).
 */
    if (kbuf[0] == '\033' && kbuf[1] == '[' && kbuf[len - 1] == '~')
	kbuf[len - 1] = (shft ? (ctrl ? '@' : '$') : (ctrl ? '^' : '~'));

/* escape prefix */
    if (meta
#ifdef META8_OPTION
	&& (meta_char == 033)
#endif
	) {
	const unsigned char ch = '\033';

	tt_write(&ch, 1);
    }
#ifdef DEBUG_CMD
    if (debug_key) {		/* Display keyboard buffer contents */
	char           *p;
	int             i;

	fprintf(stderr, "key 0x%04X [%d]: `", (unsigned int)keysym, len);
	for (i = 0, p = kbuf; i < len; i++, p++)
	    fprintf(stderr, (*p >= ' ' && *p < '\177' ? "%c" : "\\%03o"), *p);
	fprintf(stderr, "'\n");
    }
#endif				/* DEBUG_CMD */
    tt_write(kbuf, len);
}
/*}}} */

#if (MENUBAR_MAX)
/*{{{ cmd_write(), cmd_getc() */
/* attempt to `write' COUNT to the input buffer */
/* PROTO */
unsigned int
cmd_write(const unsigned char *str, unsigned int count)
{
    int             n;

    n = (count - (cmdbuf_ptr - cmdbuf_base));
/* need to insert more chars that space available in the front */
    if (n > 0) {
    /* try and get more space from the end */
	unsigned char  *src, *dst;

	dst = (cmdbuf_base + sizeof(cmdbuf_base) - 1);	/* max pointer */

	if ((cmdbuf_ptr + n) > dst)
	    n = (dst - cmdbuf_ptr);	/* max # chars to insert */

	if ((cmdbuf_endp + n) > dst)
	    cmdbuf_endp = (dst - n);	/* truncate end if needed */

    /* equiv: memmove ((cmdbuf_ptr+n), cmdbuf_ptr, n); */
	src = cmdbuf_endp;
	dst = src + n;
    /* FIXME: anything special to avoid possible pointer wrap? */
	while (src >= cmdbuf_ptr)
	    *dst-- = *src--;

    /* done */
	cmdbuf_ptr += n;
	cmdbuf_endp += n;
    }
    while (count-- && cmdbuf_ptr > cmdbuf_base) {
    /* sneak one in */
	cmdbuf_ptr--;
	*cmdbuf_ptr = str[count];
    }

    return 0;
}
#endif				/* MENUBAR_MAX */
/* cmd_getc() - Return next input character */
/*
 * Return the next input character after first passing any keyboard input
 * to the command.
 */
/* PROTO */
unsigned char
cmd_getc(void)
{
#define TIMEOUT_USEC	5000
    static short    refreshed = 0;
    fd_set          readfds;
    int             retval;

#ifdef __CYGWIN32__
    struct timeval  value;
#else
    struct itimerval value;
#endif

/* If there have been a lot of new lines, then update the screen
 * What the heck I'll cheat and only refresh less than every page-full.
 * the number of pages between refreshes is refresh_limit, which
 * is incremented here because we must be doing flat-out scrolling.
 *
 * refreshing should be correct for small scrolls, because of the
 * time-out */
    if (refresh_count >= (refresh_limit * (TermWin.nrow - 1))) {
	if (refresh_limit < REFRESH_PERIOD)
	    refresh_limit++;
	refresh_count = 0;
	refreshed = 1;
	scr_refresh(refresh_type);
    }
/* characters already read in */
    if (cmdbuf_ptr < cmdbuf_endp)
	goto Return_Char;

    for (;;) {
	v_doPending();		/* output any pending chars */
	while (XPending(Xdisplay)) {	/* process pending X events */
	    XEvent          ev;

	    refreshed = 0;
	    XNextEvent(Xdisplay, &ev);
            WMProcessEvent(app,&ev);
	    process_x_event(&ev);

	/* in case button actions pushed chars to cmdbuf */
	    if (cmdbuf_ptr < cmdbuf_endp)
		goto Return_Char;
	}
#ifndef NO_SCROLLBAR_BUTTON_CONTINUAL_SCROLLING
	if (scrollbar_isUp()) {
	    if (!scroll_arrow_delay-- && scr_page(UP, 1)) {
		scroll_arrow_delay = SCROLLBAR_CONTINUOUS_DELAY;
		refreshed = 0;
		refresh_type |= SMOOTH_REFRESH;
	    }
	} else if (scrollbar_isDn()) {
	    if (!scroll_arrow_delay-- && scr_page(DN, 1)) {
		scroll_arrow_delay = SCROLLBAR_CONTINUOUS_DELAY;
		refreshed = 0;
		refresh_type |= SMOOTH_REFRESH;
	    }
	}
#endif				/* NO_SCROLLBAR_BUTTON_CONTINUAL_SCROLLING */

    /* Nothing to do! */
	FD_ZERO(&readfds);
	FD_SET(cmd_fd, &readfds);
	FD_SET(Xfd, &readfds);
#ifdef __CYGWIN32__
	value.tv_usec = TIMEOUT_USEC;
	value.tv_sec = 0;
#else
	value.it_value.tv_usec = TIMEOUT_USEC;
	value.it_value.tv_sec = 0;
#endif

	retval = select(num_fds, &readfds, NULL, NULL,
			((refreshed
#ifndef NO_SCROLLBAR_BUTTON_CONTINUAL_SCROLLING
			  && !(scrollbar_isUpDn())
#endif
#ifdef __CYGWIN32__
			 ) ? NULL : &value));
#else
			 ) ? NULL : &value.it_value));
#endif

    /* See if we can read from the application */
	if (FD_ISSET(cmd_fd, &readfds)) {
	    unsigned int    count = BUFSIZ;

	    cmdbuf_ptr = cmdbuf_endp = cmdbuf_base;

	/* while (count > sizeof(cmdbuf_base) / 2) */
	    while (count) {
		int             n = read(cmd_fd, cmdbuf_endp, count);

		if (n <= 0)
		    break;
		cmdbuf_endp += n;
		count -= n;
	    }
	/* some characters read in */
	    if (count != BUFSIZ)
		goto Return_Char;
	}
    /* select statement timed out - better update the screen */
	if (retval == 0) {
	    refresh_count = 0;
	    refresh_limit = 1;
	    if (!refreshed) {
		refreshed = 1;
		scr_refresh(refresh_type);
		scrollbar_show(1);
	    }
	}
    }
    /* NOTREACHED */
    return 0;

  Return_Char:
    refreshed = 0;
    return (*cmdbuf_ptr++);
}
/*}}} */

/*
 * the 'essential' information for reporting Mouse Events
 * pared down from XButtonEvent
 */
static struct {
    int             clicks;
    Time            time;	/* milliseconds */
    unsigned int    state;	/* key or button mask */
    unsigned int    button;	/* detail */
} MEvent = {
    0, CurrentTime, 0, AnyButton
};

/* PROTO */
void
mouse_report(XButtonEvent * ev)
{
    int             button_number, key_state = 0;
    int             x, y;

    x = ev->x;
    y = ev->y;
    pixel_position(&x, &y);

    button_number = ((MEvent.button == AnyButton) ? 3 :
		     (MEvent.button - Button1));

    if (PrivateModes & PrivMode_MouseX10) {
    /*
     * do not report ButtonRelease
     * no state info allowed
     */
	key_state = 0;
	if (button_number == 3)
	    return;
    } else {
    /* let's be explicit here ... from <X11/X.h>
     * #define ShiftMask        (1<<0)
     * #define ControlMask        (1<<2)
     * #define Mod1Mask           (1<<3)
     *
     * and XTerm mouse reporting needs these values:
     *   4 = Shift
     *   8 = Meta
     *  16 = Control
     * plus will add in our own Double-Click reporting
     *  32 = Double Click
     */
	key_state = (((MEvent.state & (ShiftMask | ControlMask))
		      + ((MEvent.state & Mod1Mask) ? 2 : 0)
#ifdef MOUSE_REPORT_DOUBLECLICK
		      + (MEvent.clicks > 1 ? 8 : 0)
#endif
		     ) << 2);
    }

#ifdef DEBUG_MOUSEREPORT
    fprintf(stderr, "Mouse [");
    if (key_state & 16)
	fputc('C', stderr);
    if (key_state & 4)
	fputc('S', stderr);
    if (key_state & 2)
	fputc('A', stderr);
    if (key_state & 32)
	fputc('2', stderr);
    fprintf(stderr, "]: <%d>, %d/%d\n",
	    button_number,
	    x + 1,
	    y + 1);
#else
    tt_printf((unsigned char *) "\033[M%c%c%c",
	      (32 + button_number + key_state),
	      (32 + x + 1),
	      (32 + y + 1));
#endif
}

/*{{{ process an X event */
/* PROTO */
void
process_x_event(XEvent * ev)
{
    static int      bypass_keystate = 0;
    int             reportmode;
    /* for 0.20.3 lamerprophack = 5 is the correct value
     * I change this to somethine else to optimize it to wmaker 0.50.0
     */
    static int	    lameprophack = 3;
    static int      csrO = 0;	/* Hops - csr offset in thumb/slider      */
#ifdef TRANSPARENT
    static int 	    reparented = 0;
#endif
    

/*        to give proper Scroll behaviour */

    switch (ev->type) {
    case KeyPress:
	lookup_key(ev);
	break;

    case ClientMessage:
	if (ev->xclient.format == 32 && ev->xclient.data.l[0] == wmDeleteWindow)
	    exit(EXIT_SUCCESS);
#ifdef OFFIX_DND
    /* OffiX Dnd (drag 'n' drop) protocol */
	if (ev->xclient.message_type == DndProtocol &&
	    ((ev->xclient.data.l[0] == DndFile) ||
	     (ev->xclient.data.l[0] == DndDir) ||
	     (ev->xclient.data.l[0] == DndLink))) {
	/* Get Dnd data */
	    Atom            ActualType;
	    int             ActualFormat;
	    unsigned char  *data;
	    unsigned long   Size, RemainingBytes;

	    XGetWindowProperty(Xdisplay, Xroot,
			       DndSelection,
			       0L, 1000000L,
			       False, AnyPropertyType,
			       &ActualType, &ActualFormat,
			       &Size, &RemainingBytes,
			       &data);
	    XChangeProperty(Xdisplay, Xroot,
			    XA_CUT_BUFFER0, XA_STRING,
			    8, PropModeReplace,
			    data, strlen(data));
	    selection_paste(Xroot, XA_CUT_BUFFER0, True);
	    XSetInputFocus(Xdisplay, Xroot, RevertToNone, CurrentTime);
	}
#endif				/* OFFIX_DND */
	break;

    case MappingNotify:
	XRefreshKeyboardMapping(&(ev->xmapping));
	break;

    /* Here's my conclusiion:
     * If the window is completely unobscured, use bitblt's
     * to scroll. Even then, they're only used when doing partial
     * screen scrolling. When partially obscured, we have to fill
     * in the GraphicsExpose parts, which means that after each refresh,
     * we need to wait for the graphics expose or Noexpose events,
     * which ought to make things real slow!
     */
    case VisibilityNotify:
	switch (ev->xvisibility.state) {
	case VisibilityUnobscured:
	    refresh_type = FAST_REFRESH;
	    break;

	case VisibilityPartiallyObscured:
	    refresh_type = SLOW_REFRESH;
	    break;

	default:
	    refresh_type = NO_REFRESH;
	    break;
	}
	break;

    case FocusIn:
	if (!TermWin.focus) {
	    TermWin.focus = 1;
#ifndef NO_XLOCALE
	    if (Input_Context != NULL)
		XSetICFocus(Input_Context);
#endif
	}
	break;

    case FocusOut:
	if (TermWin.focus) {
	    TermWin.focus = 0;
#ifndef NO_XLOCALE
	    if (Input_Context != NULL)
		XUnsetICFocus(Input_Context);
#endif
	}
	break;

    case ConfigureNotify:
	if(lameprophack && ev->xproperty.window==TermWin.parent){
		lameprophack--;
			resize_window();
	}
	else if(Options & Opt_transparent){
		if(ParentWin1){
			resize_window();
			menubar_expose();
		}
	}
	else {
		resize_window();
		menubar_expose();
	}
	break;

    case SelectionClear:
	selection_clear();
	break;

    case SelectionNotify:
	selection_paste(ev->xselection.requestor, ev->xselection.property, True);
	break;

    case SelectionRequest:
	selection_send(&(ev->xselectionrequest));
	break;

#ifdef TRANSPARENT
     case PropertyNotify:
	{
		static Atom _xroot=0; 
		static Atom _xstate=0; 

		if(!_xstate)
		    _xstate = XInternAtom(Xdisplay, "WM_STATE", False);
		if(!_xroot)
		    _xroot = XInternAtom(Xdisplay, "_XROOTPMAP_ID", False);

		if (ev->xproperty.atom == _xroot || ev->xproperty.atom == _xstate )
		if(lameprophack && ev->xproperty.window==TermWin.parent){
			lameprophack--;
		}
		else if (Options & Opt_transparent) {
		    Window root;
		    Window parent;
		    Window *list;
		    int n;
		    Atom *props;
		    /*
		    Atom state = XInternAtom(Xdisplay, "WM_STATE", False);
		    */

		    ParentWin1 = None;
		    ParentWin2 = None;
		    
		    /*
		     * Make the frame window set by the window manager have
		     * the root background. Some window managers put 2 nested frame
		     * windows for each client, so we have to take care about that.
		     */
		    XQueryTree(Xdisplay, TermWin.parent, &root, &parent, &list, &n);
		    XFree(list);

		    if (parent == Xroot)
			break;

		    ParentWin1 = parent;
		    
		    XSetWindowBackgroundPixmap(Xdisplay, parent, ParentRelative);

		    XQueryTree(Xdisplay, ParentWin1, &root, &parent, &list, &n);
		    XFree(list);

		    if (ParentWin1){
			    scr_touch();
			    scrollbar_show(2);
		    }
		}
	}
        break;
     case UnmapNotify:
	ParentWin1 = None;
	break;
     case ReparentNotify:
     case MapNotify:
	if (Options & Opt_transparent) {
	    Window root;
	    Window parent;
	    Window *list;
	    int n;
	    Atom *props;
	    /*
	    Atom state = XInternAtom(Xdisplay, "WM_STATE", False);
	    */

	    ParentWin1 = None;
	    ParentWin2 = None;
	    
	    /*
	     * Make the frame window set by the window manager have
	     * the root background. Some window managers put 2 nested frame
	     * windows for each client, so we have to take care about that.
	     */
	    XQueryTree(Xdisplay, TermWin.parent, &root, &parent, &list, &n);
	    XFree(list);

	    if (parent == Xroot)
		break;

	    ParentWin1 = parent;
	    
	    XSetWindowBackgroundPixmap(Xdisplay, parent, ParentRelative);

	    XQueryTree(Xdisplay, ParentWin1, &root, &parent, &list, &n);
	    XFree(list);

	    if (parent != Xroot) {
		ParentWin2 = ParentWin1;
		ParentWin1 = parent;
		XSetWindowBackgroundPixmap(Xdisplay, parent, ParentRelative);
	    }
	    if (ParentWin1){
		    scr_clear();
		    scrollbar_show(2);
	    }
	}
	break;
#endif /* TRANSPARENT */

    case GraphicsExpose:
    case Expose:
	if (ev->xany.window == TermWin.vt) {
	    scr_expose(ev->xexpose.x, ev->xexpose.y,
		       ev->xexpose.width, ev->xexpose.height);
	} else {
	    XEvent          unused_xevent;


	    while (XCheckTypedWindowEvent(Xdisplay, ev->xany.window,
					  Expose,
					  &unused_xevent)) ;
	    while (XCheckTypedWindowEvent(Xdisplay, ev->xany.window,
					  GraphicsExpose,
					  &unused_xevent)) ;
	    if (isScrollbarWindow(ev->xany.window)) {

		scrollbar_setNone();
		scrollbar_show(0);
	    }
	    if (menubar_visible() && isMenuBarWindow(ev->xany.window))
		menubar_expose();
	    Gr_expose(ev->xany.window);
	}
	break;

    case ButtonPress:
	bypass_keystate = (ev->xbutton.state & (Mod1Mask | ShiftMask));
	reportmode = (bypass_keystate ?
		      0 : (PrivateModes & PrivMode_mouse_report));

	if (ev->xany.window == TermWin.vt) {
	    if (ev->xbutton.subwindow != None)
		Gr_ButtonPress(ev->xbutton.x, ev->xbutton.y);
	    else {
		if (reportmode) {
		/* mouse report from vt window */
		/* save the xbutton state (for ButtonRelease) */
		    MEvent.state = ev->xbutton.state;
#ifdef MOUSE_REPORT_DOUBLECLICK
		    if (ev->xbutton.button == MEvent.button
		     && (ev->xbutton.time - MEvent.time < MULTICLICK_TIME)) {
		    /* same button, within alloted time */
			MEvent.clicks++;
			if (MEvent.clicks > 1) {
			/* only report double clicks */
			    MEvent.clicks = 2;
			    mouse_report(&(ev->xbutton));

			/* don't report the release */
			    MEvent.clicks = 0;
			    MEvent.button = AnyButton;
			}
		    } else {
		    /* different button, or time expired */
			MEvent.clicks = 1;
			MEvent.button = ev->xbutton.button;
			mouse_report(&(ev->xbutton));
		    }
#else
		    MEvent.button = ev->xbutton.button;
		    mouse_report(&(ev->xbutton));
#endif				/* MOUSE_REPORT_DOUBLECLICK */
		} else {
		    if (ev->xbutton.button != MEvent.button)
			MEvent.clicks = 0;
		    switch (ev->xbutton.button) {
		    case Button1:
			if (MEvent.button == Button1
			&& (ev->xbutton.time - MEvent.time < MULTICLICK_TIME))
			    MEvent.clicks++;
			else
			    MEvent.clicks = 1;
			selection_click(MEvent.clicks, ev->xbutton.x,
					ev->xbutton.y);
			MEvent.button = Button1;
			break;

		    case Button3:
			if (MEvent.button == Button3
			&& (ev->xbutton.time - MEvent.time < MULTICLICK_TIME))
			    selection_rotate(ev->xbutton.x, ev->xbutton.y);
			else
			    selection_extend(ev->xbutton.x, ev->xbutton.y, 1);
			MEvent.button = Button3;
			break;
		    }
		}
		MEvent.time = ev->xbutton.time;
		return;
	    }
	}
	if (isScrollbarWindow(ev->xany.window)) {
	    scrollbar_setNone();
	/*
	 * Rxvt-style scrollbar:
	 * move up if mouse is above slider
	 * move dn if mouse is below slider
	 *
	 * XTerm-style scrollbar:
	 * Move display proportional to pointer location
	 * pointer near top -> scroll one line
	 * pointer near bot -> scroll full page
	 */
#ifndef NO_SCROLLBAR_REPORT
	    if (reportmode) {
	    /*
	     * Mouse report disabled scrollbar:
	     * arrow buttons - send up/down
	     * click on scrollbar - send pageup/down
	     */
		if (scrollbar_upButton(ev->xbutton.y))
		    tt_printf((unsigned char *) "\033[A");
		else if (scrollbar_dnButton(ev->xbutton.y))
		    tt_printf((unsigned char *) "\033[B");
		else
		    switch (ev->xbutton.button) {
		    case Button2:
			tt_printf((unsigned char *) "\014");
			break;
		    case Button1:
			tt_printf((unsigned char *) "\033[6~");
			break;
		    case Button3:
			tt_printf((unsigned char *) "\033[5~");
			break;
		    }
	    } else
#endif				/* NO_SCROLLBAR_REPORT */
	    {
		if (scrollbar_upButton(ev->xbutton.y)) {
#ifndef NO_SCROLLBAR_BUTTON_CONTINUAL_SCROLLING
		    scroll_arrow_delay = SCROLLBAR_INITIAL_DELAY;
#endif
		    if (scr_page(UP, 1))
			scrollbar_setUp();
		} else if (scrollbar_dnButton(ev->xbutton.y)) {
#ifndef NO_SCROLLBAR_BUTTON_CONTINUAL_SCROLLING
		    scroll_arrow_delay = SCROLLBAR_INITIAL_DELAY;
#endif
		    if (scr_page(DN, 1))
			scrollbar_setDn();
		} else
		    switch (ev->xbutton.button) {
		    case Button2:
#ifndef FUNKY_SCROLL_BEHAVIOUR
			csrO = (scrollBar.bot - scrollBar.top) / 2;
		    /* align to thumb center */
#else
# ifndef XTERM_SCROLLBAR
			if (scrollbar_above_slider(ev->xbutton.y) ||
			    scrollbar_below_slider(ev->xbutton.y))
# endif
#endif				/* !FUNKY_SCROLL_BEHAVIOUR */
			    scr_move_to(scrollbar_position(ev->xbutton.y) - csrO,
					scrollbar_size());
			scrollbar_setMotion();
			break;

		    case Button1:
#ifndef FUNKY_SCROLL_BEHAVIOUR
			csrO = ev->xbutton.y - scrollBar.top;
		    /* ptr ofset in thumb */
#endif
		    /*drop */

		    case Button3:
#ifndef XTERM_SCROLLBAR
			if (scrollbar_above_slider(ev->xbutton.y))
# ifdef WTERM_SCROLL_FULL
			    scr_page(UP, TermWin.nrow - 1);
# else
			    scr_page(UP, TermWin.nrow / 4);
# endif
			else if (scrollbar_below_slider(ev->xbutton.y))
# ifdef WTERM_SCROLL_FULL
			    scr_page(DN, TermWin.nrow - 1);
# else
			    scr_page(DN, TermWin.nrow / 4);
# endif
			else
			    scrollbar_setMotion();
#else				/* XTERM_SCROLLBAR */
			scr_page((ev->xbutton.button == Button1 ? DN : UP),
				 (TermWin.nrow *
				  scrollbar_position(ev->xbutton.y) /
				  scrollbar_size())
			    );
#endif				/* XTERM_SCROLLBAR */
			break;
		    }
	    }
	    return;
	}
	if (isMenuBarWindow(ev->xany.window)) {
	    menubar_control(&(ev->xbutton));
	    return;
	}
	break;

    case ButtonRelease:
	csrO = 0;		/* reset csr Offset */
	reportmode = (bypass_keystate ?
		      0 : (PrivateModes & PrivMode_mouse_report));

	if (scrollbar_isUpDn()) {
	    scrollbar_setNone();
	    scrollbar_show(0);
#ifndef NO_SCROLLBAR_BUTTON_CONTINUAL_SCROLLING
	    refresh_type &= ~SMOOTH_REFRESH;
#endif
	}
	if (ev->xany.window == TermWin.vt) {
	    if (ev->xbutton.subwindow != None)
		Gr_ButtonRelease(ev->xbutton.x, ev->xbutton.y);
	    else {
		if (reportmode) {
		/* mouse report from vt window */
#ifdef MOUSE_REPORT_DOUBLECLICK
		/* only report the release of 'slow' single clicks */
		    if (MEvent.button != AnyButton &&
			(ev->xbutton.button != MEvent.button ||
		      (ev->xbutton.time - MEvent.time > MULTICLICK_TIME / 2))
			) {
			MEvent.clicks = 0;
			MEvent.button = AnyButton;
			mouse_report(&(ev->xbutton));
		    }
#else				/* MOUSE_REPORT_DOUBLECLICK */
		    MEvent.button = AnyButton;
		    mouse_report(&(ev->xbutton));
#endif				/* MOUSE_REPORT_DOUBLECLICK */
		    return;
		}
	    /*
	     * dumb hack to compensate for the failure of click-and-drag
	     * when overriding mouse reporting
	     */
		if ((PrivateModes & PrivMode_mouse_report) &&
		    (bypass_keystate) &&
		    (ev->xbutton.button == Button1) &&
		    (MEvent.clicks <= 1))
		    selection_extend(ev->xbutton.x, ev->xbutton.y, 0);

		switch (ev->xbutton.button) {
		case Button1:
		case Button3:
		    selection_make(ev->xbutton.time);
		    break;

		case Button2:
		    selection_request(ev->xbutton.time,
				      ev->xbutton.x, ev->xbutton.y);
		    break;
#ifndef NO_MOUSE_WHEEL
		case Button4:
		    scr_page(UP, (ev->xbutton.state & ShiftMask) ? 1 : 5);
		    break;
		case Button5:
		    scr_page(DN, (ev->xbutton.state & ShiftMask) ? 1 : 5);
		    break;
#endif
		}
	    }
	} else if (isMenuBarWindow(ev->xany.window)) {
	    menubar_control(&(ev->xbutton));
	}
	break;

    case MotionNotify:
	if (isMenuBarWindow(ev->xany.window)) {
	    menubar_control(&(ev->xbutton));
	    break;
	}
	if ((PrivateModes & PrivMode_mouse_report) && !(bypass_keystate))
	    break;

	if (ev->xany.window == TermWin.vt) {
	    if ((ev->xbutton.state & (Button1Mask | Button3Mask))) {
		Window          unused_root, unused_child;
		int             unused_root_x, unused_root_y;
		unsigned int    unused_mask;

		while (XCheckTypedWindowEvent(Xdisplay, TermWin.vt,
					      MotionNotify, ev)) ;
		XQueryPointer(Xdisplay, TermWin.vt,
			      &unused_root, &unused_child,
			      &unused_root_x, &unused_root_y,
			      &(ev->xbutton.x), &(ev->xbutton.y),
			      &unused_mask);
#ifdef MOUSE_THRESHOLD
	    /* deal with a `jumpy' mouse */
		if ((ev->xmotion.time - MEvent.time) > MOUSE_THRESHOLD)
#endif
		    selection_extend((ev->xbutton.x), (ev->xbutton.y),
				  (ev->xbutton.state & Button3Mask) ? 2 : 0);
	    }
	} else if ((ev->xany.window == scrollBar.win) && scrollbar_isMotion()) {
	    Window          unused_root, unused_child;
	    int             unused_root_x, unused_root_y;
	    unsigned int    unused_mask;

	    while (XCheckTypedWindowEvent(Xdisplay, scrollBar.win,
					  MotionNotify, ev)) ;
	    XQueryPointer(Xdisplay, scrollBar.win,
			  &unused_root, &unused_child,
			  &unused_root_x, &unused_root_y,
			  &(ev->xbutton.x), &(ev->xbutton.y),
			  &unused_mask);
	    scr_move_to(scrollbar_position(ev->xbutton.y) - csrO,
			scrollbar_size());
	    scr_refresh(refresh_type);
	    refresh_count = refresh_limit = 0;
	    scrollbar_show(1);
	}
	break;
    }
}
/*}}} */

/*{{{ tt_write(), tt_printf() - output to command */
/*
 * Send count characters directly to the command
 */
/* PROTO */
void
tt_write(const unsigned char *buf, int count)
{
    v_writeBig(cmd_fd, (char *)buf, count);
#if 0
/* hangs in write with a big paste fm selection hops 19-Sep-97 */
    while (count > 0) {
	int             n = write(cmd_fd, buf, count);

	if (n > 0) {
	    count -= n;
	    buf += n;
	}
    }
#endif
}

/*
 * Send printf() formatted output to the command.
 * Only use for small ammounts of data.
 */
/* PROTO */
void
tt_printf(const unsigned char *fmt,...)
{
    static unsigned char buf[256];
    va_list         arg_ptr;

    va_start(arg_ptr, fmt);
    vsprintf((char *) buf, (char *) fmt, arg_ptr);
    va_end(arg_ptr);
    tt_write(buf, strlen(buf));
}

/*}}} */

/*{{{ print pipe */
/*----------------------------------------------------------------------*/
#ifdef PRINTPIPE
/* PROTO */
FILE           *
popen_printer(void)
{
    FILE           *stream = popen(rs_print_pipe, "w");

    if (stream == NULL)
	print_error("can't open printer pipe");
    return stream;
}

/* PROTO */
int
pclose_printer(FILE * stream)
{
    fflush(stream);
/* pclose() reported not to work on SunOS 4.1.3 */
#if defined (__sun__)		/* TODO: RESOLVE THIS */
/* pclose works provided SIGCHLD handler uses waitpid */
    return pclose(stream);	/* return fclose (stream); */
#else
    return pclose(stream);
#endif
}

/*
 * simulate attached vt100 printer
 */
/* PROTO */
void
process_print_pipe(void)
{
    int		    done;
    FILE           *fd;

    if ((fd = popen_printer()) == NULL)
	return;

/* 
 * Send all input to the printer until either ESC[4i or ESC[?4i 
 * is received. 
 */
    for (done = 0; !done;) {
	unsigned char   buf[8];
	unsigned char   ch;
	unsigned int    i, len;

	if ((ch = cmd_getc()) != '\033') {
	    if (putc(ch, fd) == EOF)
		break;		/* done = 1 */
	} else {
	    len = 0;
	    buf[len++] = ch;

	    if ((buf[len++] = cmd_getc()) == '[') {
		if ((ch = cmd_getc()) == '?') {
		    buf[len++] = '?';
		    ch = cmd_getc();
		}
		if ((buf[len++] = ch) == '4') {
		    if ((buf[len++] = cmd_getc()) == 'i')
			break;	/* done = 1 */
	        }
	    }
	    for (i = 0; i < len; i++)
		if (putc(buf[i], fd) == EOF) {
		    done = 1;
		    break;
		}
	}
    }
    pclose_printer(fd);
}
#endif				/* PRINTPIPE */
/*}}} */

/*{{{ process escape sequences */
/* PROTO */
void
process_escape_seq(void)
{
    unsigned char   ch = cmd_getc();

    switch (ch) {
    /* case 1:        do_tek_mode (); break; */
    case '#':
	if (cmd_getc() == '8')
	    scr_E();
	break;
    case '(':
	scr_charset_set(0, cmd_getc());
	break;
    case ')':
	scr_charset_set(1, cmd_getc());
	break;
    case '*':
	scr_charset_set(2, cmd_getc());
	break;
    case '+':
	scr_charset_set(3, cmd_getc());
	break;
#ifdef MULTICHAR_SET
    case '$':
	scr_charset_set(-2, cmd_getc());
	break;
#endif
    case '7':
	scr_cursor(SAVE);
	break;
    case '8':
	scr_cursor(RESTORE);
	break;
    case '=':
    case '>':
	PrivMode((ch == '='), PrivMode_aplKP);
	break;
    case '@':
	(void)cmd_getc();
	break;
    case 'D':
	scr_index(UP);
	break;
    case 'E':
	scr_add_lines((unsigned char *) "\n\r", 1, 2);
	break;
    case 'G':
	process_graphics();
	break;
    case 'H':
	scr_set_tab(1);
	break;
    case 'M':
	scr_index(DN);
	break;
    /*case 'N': scr_single_shift (2);   break; */
    /*case 'O': scr_single_shift (3);   break; */
    case 'Z':
	tt_printf((unsigned char *) ESCZ_ANSWER);
	break;			/* steal obsolete ESC [ c */
    case '[':
	process_csi_seq();
	break;
    case ']':
	process_xterm_seq();
	break;
    case 'c':
	scr_poweron();
	break;
    case 'n':
	scr_charset_choose(2);
	break;
    case 'o':
	scr_charset_choose(3);
	break;
    }
}
/*}}} */

/*{{{ process CSI (code sequence introducer) sequences `ESC[' */
/* PROTO */
void
process_csi_seq(void)
{
    unsigned char   ch, priv;
    unsigned int    nargs;
    int             arg[ESC_ARGS];

    nargs = 0;
    arg[0] = 0;
    arg[1] = 0;

    priv = 0;
    ch = cmd_getc();
    if (ch >= '<' && ch <= '?') {
	priv = ch;
	ch = cmd_getc();
    }
/* read any numerical arguments */
    do {
	int             n;

	for (n = 0; isdigit(ch); ch = cmd_getc())
	    n = n * 10 + (ch - '0');

	if (nargs < ESC_ARGS)
	    arg[nargs++] = n;
	if (ch == '\b') {
	    scr_backspace();
	} else if (ch == 033) {
	    process_escape_seq();
	    return;
	} else if (ch < ' ') {
	    scr_add_lines(&ch, 0, 1);
	    return;
	}
	if (ch < '@')
	    ch = cmd_getc();
    }
    while (ch >= ' ' && ch < '@');
    if (ch == 033) {
	process_escape_seq();
	return;
    } else if (ch < ' ')
	return;

    switch (ch) {
#ifdef PRINTPIPE
    case 'i':			/* printing */
	switch (arg[0]) {
	case 0:
	    scr_printscreen(0);
	    break;
	case 5:
	    process_print_pipe();
	    break;
	}
	break;
#endif
    case 'A':
    case 'e':			/* up <n> */
	scr_gotorc((arg[0] ? -arg[0] : -1), 0, RELATIVE);
	break;
    case 'B':			/* down <n> */
	scr_gotorc((arg[0] ? +arg[0] : +1), 0, RELATIVE);
	break;
    case 'C':
    case 'a':			/* right <n> */
	scr_gotorc(0, (arg[0] ? +arg[0] : +1), RELATIVE);
	break;
    case 'D':			/* left <n> */
	scr_gotorc(0, (arg[0] ? -arg[0] : -1), RELATIVE);
	break;
    case 'E':			/* down <n> & to first column */
	scr_gotorc((arg[0] ? +arg[0] : +1), 0, R_RELATIVE);
	break;
    case 'F':			/* up <n> & to first column */
	scr_gotorc((arg[0] ? -arg[0] : -1), 0, R_RELATIVE);
	break;
    case 'G':
    case '`':			/* move to col <n> */
	scr_gotorc(0, (arg[0] ? arg[0] - 1 : +1), R_RELATIVE);
	break;
    case 'd':			/* move to row <n> */
	scr_gotorc((arg[0] ? arg[0] - 1 : +1), 0, C_RELATIVE);
	break;
    case 'H':
    case 'f':			/* position cursor */
	switch (nargs) {
	case 0:
	    scr_gotorc(0, 0, 0);
	    break;
	case 1:
	    scr_gotorc((arg[0] ? arg[0] - 1 : 0), 0, 0);
	    break;
	default:
	    scr_gotorc(arg[0] - 1, arg[1] - 1, 0);
	    break;
	}
	break;
    case 'I':
	scr_tab(arg[0] ? +arg[0] : +1);
	break;
    case 'Z':
	scr_tab(arg[0] ? -arg[0] : -1);
	break;
    case 'J':
	scr_erase_screen(arg[0]);
	break;
    case 'K':
	scr_erase_line(arg[0]);
	break;
    case '@':
	scr_insdel_chars((arg[0] ? arg[0] : 1), INSERT);
	break;
    case 'L':
	scr_insdel_lines((arg[0] ? arg[0] : 1), INSERT);
	break;
    case 'M':
	scr_insdel_lines((arg[0] ? arg[0] : 1), DELETE);
	break;
    case 'X':
	scr_insdel_chars((arg[0] ? arg[0] : 1), ERASE);
	break;
    case 'P':
	scr_insdel_chars((arg[0] ? arg[0] : 1), DELETE);
	break;

    case 'c':
	tt_printf((unsigned char *) VT100_ANS);
	break;
    case 'm':
	process_sgr_mode(nargs, arg);
	break;
    case 'n':			/* request for information */
	switch (arg[0]) {
	case 5:
	    tt_printf((unsigned char *) "\033[0n");
	    break;		/* ready */
	case 6:
	    scr_report_position();
	    break;
#if defined (ENABLE_DISPLAY_ANSWER)
	case 7:
	    tt_printf((unsigned char *) "%s\n", display_name);
	    break;
#endif
	case 8:
	    xterm_seq(XTerm_title, APL_NAME "-" VERSION);
	    break;
	}
	break;
    case 'r':			/* set top and bottom margins */
	if (priv != '?') {
	    if (nargs < 2 || arg[0] >= arg[1])
		scr_scroll_region(0, 10000);
	    else
		scr_scroll_region(arg[0] - 1, arg[1] - 1);
	    break;
	}
    /* drop */
    case 's':
    case 't':
    case 'h':
    case 'l':
	process_terminal_mode(ch, priv, nargs, arg);
	break;
    case 'g':
	switch (arg[0]) {
	case 0:
	    scr_set_tab(0);
	    break;		/* delete tab */
	case 3:
	    scr_set_tab(-1);
	    break;		/* clear all tabs */
	}
	break;
    case 'W':
	switch (arg[0]) {
	case 0:
	    scr_set_tab(1);
	    break;		/* = ESC H */
	case 2:
	    scr_set_tab(0);
	    break;		/* = ESC [ 0 g */
	case 5:
	    scr_set_tab(-1);
	    break;		/* = ESC [ 3 g */
	}
	break;
    }
}
/*}}} */

/*{{{ process xterm text parameters sequences `ESC ] Ps ; Pt BEL' */
/* PROTO */
void
process_xterm_seq(void)
{
    unsigned char   ch, string[STRING_MAX];
    int             arg;

    ch = cmd_getc();
    for (arg = 0; isdigit(ch); ch = cmd_getc())
	arg = arg * 10 + (ch - '0');

    if (ch == ';') {
	int             n = 0;

	while ((ch = cmd_getc()) != 007) {
	    if (ch) {
		if (ch == '\t')
		    ch = ' ';	/* translate '\t' to space */
		else if (ch < ' ')
		    return;	/* control character - exit */

		if (n < sizeof(string) - 1)
		    string[n++] = ch;
	    }
	}
	string[n] = '\0';
    /*
     * menubar_dispatch() violates the constness of the string,
     * so do it here
     */
	if (arg == XTerm_Menu)
	    menubar_dispatch((char *) string);
	else
	    xterm_seq(arg, (char *) string);
    }
}
/*}}} */

/*{{{ process DEC private mode sequences `ESC [ ? Ps mode' */
/*
 * mode can only have the following values:
 *      'l' = low
 *      'h' = high
 *      's' = save
 *      'r' = restore
 *      't' = toggle
 * so no need for fancy checking
 */
/* PROTO */
void
process_terminal_mode(int mode, int priv, unsigned int nargs, int arg[])
{
    unsigned int    i;
    int             state;

    if (nargs == 0)
	return;

/* make lo/hi boolean */
    switch (mode) {
    case 'l':
	mode = 0;
	break;
    case 'h':
	mode = 1;
	break;
    }

    switch (priv) {
    case 0:
	if (mode && mode != 1)
	    return;		/* only do high/low */
	for (i = 0; i < nargs; i++)
	    switch (arg[i]) {
	    case 4:
		scr_insert_mode(mode);
		break;
	    /* case 38:  TEK mode */
	    }
	break;

#define PrivCases(bit)							\
    if (mode == 't')							\
	state = !(PrivateModes & bit);					\
    else								\
        state = mode;							\
    switch (state) {							\
    case 's':								\
	SavedModes |= (PrivateModes & bit);				\
	continue;							\
	break;								\
    case 'r':								\
	state = (SavedModes & bit) ? 1 : 0;				\
	/* FALLTHROUGH */						\
    default:								\
	PrivMode (state, bit);						\
    }

    case '?':
	for (i = 0; i < nargs; i++)
	    switch (arg[i]) {
	    case 1:		/* application cursor keys */
		PrivCases(PrivMode_aplCUR);
		break;

	    /* case 2:   - reset charsets to USASCII */

	    case 3:		/* 80/132 */
		PrivCases(PrivMode_132);
		if (PrivateModes & PrivMode_132OK)
		    set_width(state ? 132 : 80);
		break;

	    /* case 4:   - smooth scrolling */

	    case 5:		/* reverse video */
		PrivCases(PrivMode_rVideo);
		scr_rvideo_mode(state);
		break;

	    case 6:		/* relative/absolute origins  */
		PrivCases(PrivMode_relOrigin);
		scr_relative_origin(state);
		break;

	    case 7:		/* autowrap */
		PrivCases(PrivMode_Autowrap);
		scr_autowrap(state);
		break;

	    /* case 8:   - auto repeat, can't do on a per window basis */

	    case 9:		/* X10 mouse reporting */
		PrivCases(PrivMode_MouseX10);
	    /* orthogonal */
		if (PrivateModes & PrivMode_MouseX10)
		    PrivateModes &= ~(PrivMode_MouseX11);
		break;
# ifdef menuBar_esc
	    case menuBar_esc:
		PrivCases(PrivMode_menuBar);
		map_menuBar(state);
		break;
# endif
#ifdef scrollBar_esc
	    case scrollBar_esc:
		PrivCases(PrivMode_scrollBar);
		map_scrollBar(state);
		break;
#endif
	    case 25:		/* visible/invisible cursor */
		PrivCases(PrivMode_VisibleCursor);
		scr_cursor_visible(state);
		break;

	    case 35:
		PrivCases(PrivMode_ShiftKeys);
		break;

	    case 40:		/* 80 <--> 132 mode */
		PrivCases(PrivMode_132OK);
		break;

	    case 47:		/* secondary screen */
		PrivCases(PrivMode_Screen);
		scr_change_screen(state);
		break;

	    case 66:		/* application key pad */
		PrivCases(PrivMode_aplKP);
		break;

	    case 67:
#ifndef NO_BACKSPACE_KEY
		if (PrivateModes & PrivMode_HaveBackSpace)
		    PrivCases(PrivMode_BackSpace);
#endif
		break;

	    case 1000:		/* X11 mouse reporting */
		PrivCases(PrivMode_MouseX11);
	    /* orthogonal */
		if (PrivateModes & PrivMode_MouseX11)
		    PrivateModes &= ~(PrivMode_MouseX10);
		break;
#if 0
	    case 1001:
		break;		/* X11 mouse highlighting */
#endif
	    case 1010:		/* scroll to bottom on TTY output inhibit */
		PrivCases(PrivMode_TtyOutputInh);
		if (PrivateModes & PrivMode_TtyOutputInh)
		    Options |= Opt_scrollTtyOutputInh;
		else
		    Options &= ~Opt_scrollTtyOutputInh;
		break;
	    case 1011:		/* scroll to bottom on key press */
		PrivCases(PrivMode_Keypress);
		if (PrivateModes & PrivMode_Keypress)
		    Options |= Opt_scrollKeypress;
		else
		    Options &= ~Opt_scrollKeypress;
		break;
	    }
#undef PrivCases
	break;
    }
}
/*}}} */

/*{{{ process sgr sequences */
/* PROTO */
void
process_sgr_mode(unsigned int nargs, int arg[])
{
    unsigned int    i;

    if (nargs == 0) {
	scr_rendition(0, ~RS_None);
	return;
    }
    for (i = 0; i < nargs; i++)
	switch (arg[i]) {
	case 0:
	    scr_rendition(0, ~RS_None);
	    break;
	case 1:
	    scr_rendition(1, RS_Bold);
	    break;
	case 4:
	    scr_rendition(1, RS_Uline);
	    break;
	case 5:
	    scr_rendition(1, RS_Blink);
	    break;
	case 7:
	    scr_rendition(1, RS_RVid);
	    break;
	case 22:
	    scr_rendition(0, RS_Bold);
	    break;
	case 24:
	    scr_rendition(0, RS_Uline);
	    break;
	case 25:
	    scr_rendition(0, RS_Blink);
	    break;
	case 27:
	    scr_rendition(0, RS_RVid);
	    break;

	case 30:
	case 31:		/* set fg color */
	case 32:
	case 33:
	case 34:
	case 35:
	case 36:
	case 37:
	    scr_color(minCOLOR + (arg[i] - 30), RS_Bold);
	    break;
	case 39:		/* default fg */
	    scr_color(restoreFG, RS_Bold);
	    break;

	case 40:
	case 41:		/* set bg color */
	case 42:
	case 43:
	case 44:
	case 45:
	case 46:
	case 47:
	    scr_color(minCOLOR + (arg[i] - 40), RS_Blink);
	    break;
	case 49:		/* default bg */
	    scr_color(restoreBG, RS_Blink);
	    break;
	}
}
/*}}} */

/*{{{ process Rob Nation's own graphics mode sequences */
/* PROTO */
void
process_graphics(void)
{
    unsigned char   ch, cmd = cmd_getc();

#ifndef WTERM_GRAPHICS
    if (cmd == 'Q') {		/* query graphics */
	tt_printf((unsigned char *) "\033G0\n");	/* no graphics */
	return;
    }
/* swallow other graphics sequences until terminating ':' */
    do
	ch = cmd_getc();
    while (ch != ':');
#else
    int             nargs;
    int             args[NGRX_PTS];
    unsigned char  *text = NULL;

    if (cmd == 'Q') {		/* query graphics */
	tt_printf((unsigned char *) "\033G1\n");	/* yes, graphics (color) */
	return;
    }
    for (nargs = 0; nargs < (sizeof(args) / sizeof(args[0])) - 1; ) {
	int             neg;

	ch = cmd_getc();
	neg = (ch == '-');
	if (neg || ch == '+')
	    ch = cmd_getc();

	for (args[nargs] = 0; isdigit(ch); ch = cmd_getc())
	    args[nargs] = args[nargs] * 10 + (ch - '0');
	if (neg)
	    args[nargs] = -args[nargs];

	nargs++;
	args[nargs] = 0;
	if (ch != ';')
	    break;
    }

    if ((cmd == 'T') && (nargs >= 5)) {
	int             i, len = args[4];

	text = MALLOC((len + 1) * sizeof(char));

	if (text != NULL) {
	    for (i = 0; i < len; i++)
		text[i] = cmd_getc();
	    text[len] = '\0';
	}
    }
    Gr_do_graphics(cmd, nargs, args, text);
#endif
}
/*}}} */

/*{{{ Read and process output from the application */
/* PROTO */
void
main_loop(void)
{
    int             ch;

    do {
	while ((ch = cmd_getc()) == 0) ;	/* wait for something */
	if (ch >= ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
	/* Read a text string from the input buffer */
	    int             nlines = 0;
	    unsigned char  *str;

	/*
	 * point to the start of the string,
	 * decrement first since it was post incremented in cmd_getc()
	 */
	    str = --cmdbuf_ptr;
	    while (cmdbuf_ptr < cmdbuf_endp) {
		ch = *cmdbuf_ptr++;
		if (ch >= ' ' || ch == '\t' || ch == '\r') {
		/* nothing */
		} else if (ch == '\n') {
		    nlines++;
		    if (++refresh_count >= (refresh_limit * (TermWin.nrow - 1)))
			break;
		} else {	/* unprintable */
		    cmdbuf_ptr--;
		    break;
		}
	    }
	    scr_add_lines(str, nlines, (cmdbuf_ptr - str));
	} else {
	    switch (ch) {
	    case 005:		/* terminal Status */
		tt_printf((unsigned char *) VT100_ANS);
		break;
	    case 007:		/* bell */
		scr_bell();
		break;
	    case '\b':		/* backspace */
		scr_backspace();
		break;
	    case 013:		/* vertical tab, form feed */
	    case 014:
		scr_index(UP);
		break;
	    case 016:		/* shift out - acs */
		scr_charset_choose(1);
		break;
	    case 017:		/* shift in - acs */
		scr_charset_choose(0);
		break;
	    case 033:		/* escape char */
		process_escape_seq();
		break;
	    }
	}
    } while (ch != EOF);
}

/* ---------------------------------------------------------------------- */
/* Addresses pasting large amounts of data and wterm hang
 * code pinched from xterm and applied originally to wterm-2.18
 * Hops
 */

static char    *v_buffer;	/* pointer to physical buffer */
static char    *v_bufstr = NULL;	/* beginning of area to write */
static char    *v_bufptr;	/* end of area to write */
static char    *v_bufend;	/* end of physical buffer */

/* output a burst of any pending data fm a paste... */
/* PROTO */
int
v_doPending(void)
{
    if (v_bufstr >= v_bufptr)
	return 0;
    v_writeBig(cmd_fd, NULL, 0);
    return 1;
}

/* Write data to the pty as typed by the user, pasted with the mouse,
 * or generated by us in response to a query ESC sequence.
 * Code Pinched from xterm 
 */
/* PROTO */
void
v_writeBig(int f, char *d, int len)
{
    int             riten;

    if (v_bufstr == NULL && len > 0) {
	v_buffer = v_bufstr = v_bufptr = MALLOC(len);
	v_bufend = v_buffer + len;
    }
/*
 * Append to the block we already have.  Always doing this simplifies the
 * code, and isn't too bad, either.  If this is a short block, it isn't
 * too expensive, and if this is a long block, we won't be able to write
 * it all anyway.
 */
    if (len > 0) {
	if (v_bufend < v_bufptr + len) {	/* we've run out of room */
	    if (v_bufstr != v_buffer) {
	    /* there is unused space, move everything down */
	    /* possibly overlapping bcopy here */
	    /* bcopy(v_bufstr, v_buffer, v_bufptr - v_bufstr); */
		memcpy(v_buffer, v_bufstr, v_bufptr - v_bufstr);
		v_bufptr -= v_bufstr - v_buffer;
		v_bufstr = v_buffer;
	    }
	    if (v_bufend < v_bufptr + len) {
	    /* still won't fit: get more space */
	    /* Don't use XtRealloc because an error is not fatal. */
		int             size = v_bufptr - v_buffer;

	    /* save across realloc */
		v_buffer = REALLOC(v_buffer, size + len);
		if (v_buffer) {
		    v_bufstr = v_buffer;
		    v_bufptr = v_buffer + size;
		    v_bufend = v_bufptr + len;
		} else {
		/* no memory: ignore entire write request */
		    fprintf(stderr, "wterm: cannot allocate buffer space\n");
		    v_buffer = v_bufstr;	/* restore clobbered pointer */
		}
	    }
	}
	if (v_bufend >= v_bufptr + len) {	/* new stuff will fit */
	    memcpy(v_bufptr, d, len);	/* bcopy(d, v_bufptr, len); */
	    v_bufptr += len;
	}
    }
/*
 * Write out as much of the buffer as we can.
 * Be careful not to overflow the pty's input silo.
 * We are conservative here and only write a small amount at a time.
 *
 * If we can't push all the data into the pty yet, we expect write
 * to return a non-negative number less than the length requested
 * (if some data written) or -1 and set errno to EAGAIN,
 * EWOULDBLOCK, or EINTR (if no data written).
 *
 * (Not all systems do this, sigh, so the code is actually
 * a little more forgiving.)
 */

#define MAX_PTY_WRITE 128	/* 1/2 POSIX minimum MAX_INPUT */

    if (v_bufptr > v_bufstr) {
	riten = write(f, v_bufstr, v_bufptr - v_bufstr <= MAX_PTY_WRITE ?
		      v_bufptr - v_bufstr : MAX_PTY_WRITE);
	if (riten < 0) {
	    riten = 0;
	}
	v_bufstr += riten;
	if (v_bufstr >= v_bufptr)	/* we wrote it all */
	    v_bufstr = v_bufptr = v_buffer;
    }
/*
 * If we have lots of unused memory allocated, return it
 */
    if (v_bufend - v_bufptr > 1024) {	/* arbitrary hysteresis */
    /* save pointers across realloc */
	int             start = v_bufstr - v_buffer;
	int             size = v_bufptr - v_buffer;
	int             allocsize = size ? size : 1;

	v_buffer = REALLOC(v_buffer, allocsize);
	if (v_buffer) {
	    v_bufstr = v_buffer + start;
	    v_bufptr = v_buffer + size;
	    v_bufend = v_buffer + allocsize;
	} else {
	/* should we print a warning if couldn't return memory? */
	    v_buffer = v_bufstr - start;	/* restore clobbered pointer */
	}
    }
}

/*}}} */
/*----------------------- end-of-file (C source) -----------------------*/
