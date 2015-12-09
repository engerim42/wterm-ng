/* ------------------------------------------------------------------------- */
/* Set TERMINFO value to the value given by configure */
#undef WTERM_TERMINFO

/* Set TERM to the value given by configure */
#undef TERMENV

/* Define if you want to use your system's memset() */
#undef NO_RMEMSET

/* Define if you don't want any resources read */
#undef NO_RESOURCES 

/* Define if you want to use XGetDefault instead of our internal version
 * which only reads ~/.Xdefaults, if it exists, otherwise ~/.Xresources if it
 * exists, and saves 60-150kB memory */
#undef USE_XGETDEFAULT

/* Define to use old rxvt (ver 2.20 and before) style selection, not xterm
 * style.  OLD_SELECTION implies OLD_WORD_SELECTION */
#undef OLD_SELECTION

/* Define to use old word selection (double click) style for you older users */
#undef OLD_WORD_SELECTION

/* Define if you want the depth of scrollbars and menus to be less
 * (width of 3d-look shadows and highlights)  --pjh */
#undef HALFSHADOW

/* Define if you want KANJI support */
/* after compilation, rename executable as `kxvt' */
#undef KANJI

/* Define if you want chinese support */
/* after compilation, rename executable as `cwterm' */
#undef ZH

/* Define if Xlocale support doesn't work */
#undef NO_XLOCALE

/* Define is setlocale (defined to Xsetlocale) doesn't work */
#undef NO_SETLOCALE

/* Define if you want Menubar support */
#undef MENUBAR

/* Define if you don't want support for the backspace key */
#undef NO_BACKSPACE_KEY

/* Define if you don't want support for the (non-keypad) delete key */
#undef NO_DELETE_KEY

/* Define if you want Rob Nation's own graphic mode */
#undef WTERM_GRAPHICS

/* Define if you want to use NeXT style scrollbars */
#undef NEXT_SCROLLBAR

/* Define if you want to revert to Xterm style scrollbars */
#undef XTERM_SCROLLBAR

/* Define if you want support for Greek Elot-928 & IBM-437 keyboard */
/* see doc/README.greek */
#undef GREEK_SUPPORT

/* Define if you want tty's to be setgid() to the `tty' group */
#undef TTY_GID_SUPPORT

/* Define if you want to have utmp/utmpx support */
#undef UTMP_SUPPORT

/* Define if you want to have wtmp support when utmp/utmpx is enabled */
#undef WTMP_SUPPORT

/* Define if you want to have sexy-looking background pixmaps. Needs libXpm */
#undef XPM_BACKGROUND

/* Define if you want to have a transparent background */
#undef TRANSPARENT

/* Define if you include <X11/xpm.h> on a normal include path (be careful) */
#undef XPM_INC_X11

/* Disable the secondary screen ("\E[?47h" / "\E[?47l")
 * Many programs use the secondary screen as their workplace. The advantage
 * is, that when you exit those programs, your previous screen contents (in
 * general the shell as you left it) will be shown again. */
#undef NO_SECONDARY_SCREEN

/* Define if you want continual scrolling on when you keep the
 * scrollbar button pressed */
#undef NO_SCROLLBAR_BUTTON_CONTINUAL_SCROLLING

/*
 * Use wheel events (button4 and button5) to scroll.  Even if you don't
 * have a wheeled mouse, this is harmless unless you have an exotic mouse!
 */
#undef NO_MOUSE_WHEEL

/* ------------------------------------------------------------------------- */
/* Define if struct utmp/utmpx contains ut_host */
#undef HAVE_UTMP_HOST

/* Define location of utmp/utmpx */
#undef WTERM_UTMP_FILE

/* Define location of wtmp */
#undef WTERM_WTMP_FILE

/* Define location of ttys/ttytab */
#undef TTYTAB_FILENAME

/* Define if you need function prototypes */
#undef PROTOTYPES

/* type of (normal and unsigned) 16 bit, 32 bit and pointer */
#undef RINT16T
#undef RUINT16T
#undef RINT32T
#undef RUINT32T
#undef RINTPT
#undef RUINTPT
