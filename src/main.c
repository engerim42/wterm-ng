#include <WINGs/WINGs.h>
#include <WMaker.h>


/*--------------------------------*-C-*---------------------------------*
 * File:	main.c
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
 *---------------------------------------------------------------------*/
/*---------------------------------------------------------------------*
 * Originally written:
 *    1992      John Boyey, University of Canterbury
 * Modifications:
 *    1994      Robert Nation <nation@rocket.sanders.lockheed.com>
 *              - extensive modifications
 *    1995      Garrett D'Amore <garrett@netcom.com>
 *    1997      mj olesen <olesen@me.QueensU.CA>
 *              - extensive modifications
 *    1997,1998 Oezguer Kesim <kesim@math.fu-berlin.de>
 *    1998      Geoff Wing <gcw@pobox.com>
 *----------------------------------------------------------------------*/

#ifndef lint
static const char rcsid[] = "$Id: main.c,v 1.18 1998/09/24 19:22:44 mason Exp $";
#endif

#define INTERN			/* assign all global vars to me */
#include "wterm.h"		/* NECESSARY */
 

WMMenu *menu;
WMMenu *menu_shade;
WMMenu *menu_background;
WMMenu *menu_shade_policy;
WMMenu *menu_shade_policy_advance;
WMMenu *menu_font;
WMMenu *menu_font_color;
WMAppContext *app;
Pixel initfg;
Pixel initbg;

/*{{{ extern functions referenced */
#ifdef DISPLAY_IS_IP
extern char    *network_display(const char *display);
#endif
/*}}} */

/*{{{ local variables */
static Cursor   TermWin_cursor;	/* cursor for vt window */

static XSizeHints szHint =
{
    PMinSize | PResizeInc | PBaseSize | PWinGravity,
    0, 0, 80, 24,		/* x, y, width, height */
    1, 1,			/* Min width, height */
    0, 0,			/* Max width, height - unused */
    1, 1,			/* increments: width, height */
    {1, 1},			/* increments: x, y */
    {0, 0},			/* Aspect ratio - unused */
    0, 0,			/* base size: width, height */
    NorthWestGravity		/* gravity */
};

static const char *def_colorName[] =
{
    "Black", "White",		/* fg/bg */
/* low-intensity colors */
    "Black",			/* 0: black             (#000000) */
#ifndef NO_BRIGHTCOLOR
    "Red3",			/* 1: red               (#CD0000) */
    "Green3",			/* 2: green             (#00CD00) */
    "Yellow3",			/* 3: yellow            (#CDCD00) */
    "Blue3",			/* 4: blue              (#0000CD) */
    "Magenta3",			/* 5: magenta           (#CD00CD) */
    "Cyan3",			/* 6: cyan              (#00CDCD) */
    "AntiqueWhite",		/* 7: white             (#FAEBD7) */
/* high-intensity colors */
    "Grey25",			/* 8: bright black      (#404040) */
#endif				/* NO_BRIGHTCOLOR */
    "Red",			/* 1/9: bright red      (#FF0000) */
    "Green",			/* 2/10: bright green   (#00FF00) */
    "Yellow",			/* 3/11: bright yellow  (#FFFF00) */
    "Blue",			/* 4/12: bright blue    (#0000FF) */
    "Magenta",			/* 5/13: bright magenta (#FF00FF) */
    "Cyan",			/* 6/14: bright cyan    (#00FFFF) */
    "White",			/* 7/15: bright white   (#FFFFFF) */
#ifndef NO_CURSORCOLOR
    NULL, NULL,
#endif				/* NO_CURSORCOLOR */
    NULL,			/* pointerColor                   */
    NULL			/* borderColor                    */
#ifndef NO_BOLDUNDERLINE
  , NULL, NULL
#endif				/* NO_BOLDUNDERLINE */
#ifdef KEEP_SCROLLCOLOR
  , "#B2B2B2",			/* scrollColor: `match' Netscape color */
    "#969696"			/* troughColor */
#endif
};

#ifdef MULTICHAR_SET
/* Multicharacter font names, roman fonts sized to match */
static const char *def_mfontName[] =
{
    MFONT_LIST
};
#endif				/* MULTICHAR_SET */

static const char *def_fontName[] =
{
    NFONT_LIST
};

/*}}} */

/* have we changed the font? Needed to avoid raceconditions
 * while window resizing                                 */
static int      font_change_count = 0;

/*----------------------------------------------------------------------*/
/* ARGSUSED */
/* PROTO */
XErrorHandler
xerror_handler(Display * display, XErrorEvent * event)
{

    print_error("XError: Request: %d . %d, Error: %d", event->request_code,
		event->minor_code, event->error_code);
    exit(EXIT_FAILURE);
    return 0;
}

/*{{{ color aliases, fg/bg bright-bold */
/* PROTO */
void
color_aliases(int idx)
{
    if (rs_color[idx] && isdigit(*rs_color[idx])) {
	int             i = atoi(rs_color[idx]);

	if (i >= 8 && i <= 15) {	/* bright colors */
	    i -= 8;
#ifndef NO_BRIGHTCOLOR
	    rs_color[idx] = rs_color[minBrightCOLOR + i];
	    return;
#endif
	}
	if (i >= 0 && i <= 7)	/* normal colors */
	    rs_color[idx] = rs_color[minCOLOR + i];
    }
}

/*{{{ exit menu */
/* PROTO */
static void 
callmenu_clone(char **glob_argv, int item, Time time)
{
	if(!fork()) execvp(glob_argv[0], &glob_argv[0]);
}
/*}}} */

/*{{{ exit menu */
/* PROTO */
static void 
callmenu_exit(void *foo, int item, Time time)
{
	exit(1);
}
/*}}} */

/*{{{ exit menu */
/* PROTO */
static void 
callmenu_restorecolor()
{
	PixColors[Color_fg]=initfg;
	PixColors[Color_bg]=initbg;
	scr_touch();
	scr_touch();
}
/*}}} */

/*{{{ exit menu */
/* PROTO */
static void 
callmenu_swapcolor(char **glob_argv, int item, Time time)
{
	Pixel swap;
	swap=PixColors[Color_fg];
	PixColors[Color_fg]=PixColors[Color_bg];
	PixColors[Color_bg]=swap;
	scr_touch();
	scr_touch();
}
/*}}} */

/*{{{ tint change menu */
/* PROTO */
static void 
callmenu_changefontcolor(void *foo, int item, Time time)
{
	XGCValues       gcvalue;

        gcvalue.foreground = PixColors[(int)foo];
        gcvalue.background = PixColors[Color_White];
	gcvalue.graphics_exposures = 0;

	XChangeGC(Xdisplay, TermWin.stippleGCfg, GCForeground|GCBackground|
			GCGraphicsExposures , &gcvalue);

	PixColors[Color_fg] = PixColors[(int)foo];
	scr_touch();
	scr_touch();
	scrollbar_show(2);
}
/*}}} */

/*{{{ tint change menu */
/* PROTO */
static void 
callmenu_changebackgroundcolor(void *foo, int item, Time time)
{
	PixColors[Color_bg] = PixColors[(int)foo];
	scr_touch();
	scr_touch();
}
/*}}} */

/*{{{ tint change menu */
/* PROTO */
static void 
callmenu_changefont(void *foo, int item, Time time)
{
	switch((int)foo){
		case 1:
	        change_font(0, FONT_UP);
		break;
		case 2:
	        change_font(0, FONT_DN);
		break; 
		case 3:
	        change_font(0, "-etl-*-*-*-*-*-16-*-*-*-*-*-*-*");
		break;
	}
}

/*}}} */

/*{{{ tint change menu */
/* PROTO */
static void 
callmenu_policy(void *foo, int item, Time time)
{
	XGCValues       gcvalue;

        gcvalue.function = (int)foo;

	XChangeGC(Xdisplay, TermWin.stippleGC, GCFunction, &gcvalue);
	scr_touch();
	scrollbar_show(2);
}
/*}}} */

/*{{{ tint change menu */
/* PROTO */
static void 
callmenu_changecolor(void *foo, int item, Time time)
{

	XGCValues       gcvalue;

        gcvalue.foreground = PixColors[(int)foo];
        gcvalue.background = PixColors[Color_White];
	gcvalue.graphics_exposures = 0;

	XChangeGC(Xdisplay, TermWin.stippleGC, GCForeground|GCBackground|
			GCGraphicsExposures , &gcvalue);
	PixColors[Color_bg] = PixColors[(int)foo];
	scr_touch();
	scrollbar_show(2);

}
/*}}} */

/*
 * find if fg/bg matches any of the normal (low-intensity) colors
 */
#ifndef NO_BRIGHTCOLOR
/* PROTO */
void
set_colorfgbg(void)
{
    unsigned int    i;
    static char     colorfgbg_env[] = "COLORFGBG=default;default;bg";
    char           *p;
    int             fg = -1, bg = -1;

    for (i = Color_Black; i <= Color_White; i++) {
	if (PixColors[Color_fg] == PixColors[i]) {
	    fg = (i - Color_Black);
	    break;
	}
    }
    for (i = Color_Black; i <= Color_White; i++) {
	if (PixColors[Color_bg] == PixColors[i]) {
	    bg = (i - Color_Black);
	    break;
	}
    }

    p = strchr(colorfgbg_env, '=');
    p++;
    if (fg >= 0)
	sprintf(p, "%d;", fg);
    else
	STRCPY(p, "default;");
    p = strchr(p, '\0');
    if (bg >= 0)
	sprintf(p,
# ifdef XPM_BACKGROUND
		"default;"
# endif
		"%d", bg);
    else
	STRCPY(p, "default");
    putenv(colorfgbg_env);

    colorfgbg = DEFAULT_RSTYLE;
    for (i = minCOLOR; i <= maxCOLOR; i++) {
	if (PixColors[Color_fg] == PixColors[i]
# ifndef NO_BOLDUNDERLINE
	    && PixColors[Color_fg] == PixColors[Color_BD]
# endif				/* NO_BOLDUNDERLINE */
    /* if we wanted boldFont to have precedence */
# if 0				/* ifndef NO_BOLDFONT */
	    && TermWin.boldFont == NULL
# endif				/* NO_BOLDFONT */
	    )
	    colorfgbg = SET_FGCOLOR(colorfgbg, i);
	if (PixColors[Color_bg] == PixColors[i])
	    colorfgbg = SET_BGCOLOR(colorfgbg, i);
    }
}
#else				/* NO_BRIGHTCOLOR */
# define set_colorfgbg() ((void)0)
#endif				/* NO_BRIGHTCOLOR */
/*}}} */

/*{{{ Create_Windows() - Open and map the window */
/* PROTO */
void
Create_Windows(int argc, char *argv[])
{
    Cursor          cursor;
    XClassHint      classHint;
    XWMHints        wmHint;
    int             i, x, y, flags;
    unsigned int    width, height;

#ifdef PREFER_24BIT
    XSetWindowAttributes attributes;
#endif

/*
 * grab colors before netscape does
 */
    for (i = 0;
	 i < (Xdepth <= 2 ? 2 : NRS_COLORS);
	 i++) {
	const char     *const msg = "can't load color \"%s\"";
	XColor          xcol;

	if (!rs_color[i])
	    continue;

	if (!XParseColor(Xdisplay, Xcmap, rs_color[i], &xcol) ||
	    !XAllocColor(Xdisplay, Xcmap, &xcol)) {
	    print_error(msg, rs_color[i]);
	    rs_color[i] = def_colorName[i];
	    if (!rs_color[i])
		continue;
	    if (!XParseColor(Xdisplay, Xcmap, rs_color[i], &xcol) ||
		!XAllocColor(Xdisplay, Xcmap, &xcol)) {
		print_error(msg, rs_color[i]);
		switch (i) {
		case Color_fg:
		case Color_bg:
		/* fatal: need bg/fg color */
		    print_error("aborting");
		    exit(EXIT_FAILURE);
		    break;
#ifndef NO_CURSORCOLOR
		case Color_cursor:
		    xcol.pixel = PixColors[Color_bg];
		    break;
		case Color_cursor2:
		    xcol.pixel = PixColors[Color_fg];
		    break;
#endif				/* NO_CURSORCOLOR */
		case Color_pointer:
		    xcol.pixel = PixColors[Color_fg];
		    break;
		default:
		    xcol.pixel = PixColors[Color_bg];	/* None */
		    break;
		}
	    }
	}
	PixColors[i] = xcol.pixel;
    }

    if (Xdepth <= 2 || !rs_color[Color_pointer])
	PixColors[Color_pointer] = PixColors[Color_fg];
    if (Xdepth <= 2 || !rs_color[Color_border])
	PixColors[Color_border] = PixColors[Color_fg];

/*
 * get scrollBar/menuBar shadow colors
 *
 * The calculations of topShadow/bottomShadow values are adapted
 * from the fvwm window manager.
 */
#ifdef KEEP_SCROLLCOLOR
    if (Xdepth <= 2) {		/* Monochrome */
	PixColors[Color_scroll] = PixColors[Color_fg];
	PixColors[Color_topShadow] = PixColors[Color_bg];
	PixColors[Color_bottomShadow] = PixColors[Color_bg];
    } else {
	XColor          xcol, white;

    /* bottomShadowColor */
	xcol.pixel = PixColors[Color_scroll];
	XQueryColor(Xdisplay, Xcmap, &xcol);

	xcol.red = ((xcol.red) / 2);
	xcol.green = ((xcol.green) / 2);
	xcol.blue = ((xcol.blue) / 2);

	if (!XAllocColor(Xdisplay, Xcmap, &xcol)) {
	    print_error("can't allocate %s", "Color_bottomShadow");
	    xcol.pixel = PixColors[minCOLOR];
	}
	PixColors[Color_bottomShadow] = xcol.pixel;

    /* topShadowColor */
# ifdef PREFER_24BIT
	white.red = white.green = white.blue = (unsigned short) ~0;
	XAllocColor(Xdisplay, Xcmap, &white);
/*        XFreeColors(Xdisplay, Xcmap, &white.pixel, 1, ~0); */
# else
	white.pixel = WhitePixel(Xdisplay, Xscreen);
	XQueryColor(Xdisplay, Xcmap, &white);
# endif

	xcol.pixel = PixColors[Color_scroll];
	XQueryColor(Xdisplay, Xcmap, &xcol);

	xcol.red = max((white.red / 5), xcol.red);
	xcol.green = max((white.green / 5), xcol.green);
	xcol.blue = max((white.blue / 5), xcol.blue);

	xcol.red = min(white.red, (xcol.red * 7) / 5);
	xcol.green = min(white.green, (xcol.green * 7) / 5);
	xcol.blue = min(white.blue, (xcol.blue * 7) / 5);

	if (!XAllocColor(Xdisplay, Xcmap, &xcol)) {
	    print_error("can't allocate %s", "Color_topShadow");
	    xcol.pixel = PixColors[Color_White];
	}
	PixColors[Color_topShadow] = xcol.pixel;
    }
#endif				/* KEEP_SCROLLCOLOR */

    szHint.base_width = (2 * TermWin_internalBorder +
			 (Options & Opt_scrollBar ? (SB_WIDTH + 2 * sb_shadow)
			  : 0));
    szHint.base_height = (2 * TermWin_internalBorder);

    flags = (rs_geometry ?
	     XParseGeometry(rs_geometry, &x, &y, &width, &height) : 0);

    if (flags & WidthValue) {
	szHint.width = width;
	szHint.flags |= USSize;
    }
    if (flags & HeightValue) {
	szHint.height = height;
	szHint.flags |= USSize;
    }
    TermWin.ncol = szHint.width;
    TermWin.nrow = szHint.height;

    change_font(1, NULL);

    { /* ONLYIF MENUBAR */
	szHint.base_height += (delay_menu_drawing ? menuBar_TotalHeight() : 0);
    }

    if (flags & XValue) {
	if (flags & XNegative) {
	    x += (DisplayWidth(Xdisplay, Xscreen)
		  - (szHint.width + TermWin_internalBorder));
	    szHint.win_gravity = NorthEastGravity;
	}
	szHint.x = x;
	szHint.flags |= USPosition;
    }
    if (flags & YValue) {
	if (flags & YNegative) {
	    y += (DisplayHeight(Xdisplay, Xscreen)
		  - (szHint.height + TermWin_internalBorder));
	    szHint.win_gravity = (szHint.win_gravity == NorthEastGravity ?
				  SouthEastGravity : SouthWestGravity);
	}
	szHint.y = y;
	szHint.flags |= USPosition;
    }
/* parent window - reverse video so we can see placement errors
 * sub-window placement & size in resize_subwindows()
 */

#ifdef PREFER_24BIT
    attributes.background_pixel = PixColors[Color_fg];
    attributes.border_pixel = PixColors[Color_fg];
    attributes.colormap = Xcmap;
    TermWin.parent = XCreateWindow(Xdisplay, Xroot,
				   szHint.x, szHint.y,
				   szHint.width, szHint.height,
				   BORDERWIDTH,
				   Xdepth, InputOutput,
				   Xvisual,
				   CWBackPixel | CWBorderPixel | CWColormap,
				   &attributes);
#else
    TermWin.parent = XCreateSimpleWindow(Xdisplay, Xroot,
					 szHint.x, szHint.y,
					 szHint.width, szHint.height,
					 BORDERWIDTH,
					 PixColors[Color_bg],
					 PixColors[Color_fg]);
#endif
    xterm_seq(XTerm_title, rs_title);
    xterm_seq(XTerm_iconName, rs_iconName);
/* ignore warning about discarded `const' */
    classHint.res_name = (char *)rs_name;
    classHint.res_class = APL_CLASS;
    wmHint.input = True;
    wmHint.initial_state = (Options & Opt_iconic ? IconicState : NormalState);
    wmHint.window_group = TermWin.parent;
    wmHint.flags = (InputHint | StateHint | WindowGroupHint);

    XSetWMProperties(Xdisplay, TermWin.parent, NULL, NULL, argv, argc,
		     &szHint, &wmHint, &classHint);

    XSelectInput(Xdisplay, TermWin.parent,
		 (KeyPressMask | FocusChangeMask | PropertyChangeMask |
		  StructureNotifyMask | VisibilityChangeMask )
	);
        XSelectInput( Xdisplay, Xroot, PropertyChangeMask );

/* vt cursor: Black-on-White is standard, but this is more popular */
    TermWin_cursor = XCreateFontCursor(Xdisplay, XC_xterm);
    {
	XColor          fg, bg;

	fg.pixel = PixColors[Color_pointer];
	XQueryColor(Xdisplay, Xcmap, &fg);
	bg.pixel = PixColors[Color_bg];
	XQueryColor(Xdisplay, Xcmap, &bg);
	XRecolorCursor(Xdisplay, TermWin_cursor, &fg, &bg);
    }

/* cursor (menuBar/scrollBar): Black-on-White */
    cursor = XCreateFontCursor(Xdisplay, XC_left_ptr);

/* the vt window */
    TermWin.vt = XCreateSimpleWindow(Xdisplay, TermWin.parent,
				     0, 0,
				     szHint.width, szHint.height,
				     0,
				     PixColors[Color_fg],
				     PixColors[Color_bg]);

    XDefineCursor(Xdisplay, TermWin.vt, TermWin_cursor);
    XSelectInput(Xdisplay, TermWin.vt,
		 (ExposureMask | ButtonPressMask | ButtonReleaseMask |
		  Button1MotionMask | Button3MotionMask));

#ifdef TRANSPARENT
    if (Options & Opt_transparent) {
	XSetWindowBackgroundPixmap(Xdisplay, TermWin.parent, ParentRelative);
	XSetWindowBackgroundPixmap(Xdisplay, TermWin.vt, ParentRelative);
    }
#endif

    XMapWindow(Xdisplay, TermWin.vt);
    XMapWindow(Xdisplay, TermWin.parent);
    app=WMAppCreateWithMain(Xdisplay, DefaultScreen(Xdisplay), TermWin.parent);
    if(Options & Opt_wmmenu){
	    menu=WMMenuCreate(app, (char *)rs_name);
	    menu_shade=WMMenuCreate(app, "Shades");
	    menu_background=WMMenuCreate(app, "Colors");
	    menu_shade_policy=WMMenuCreate(app, "Types");
	    menu_shade_policy_advance=WMMenuCreate(app, "Graphic Context Types");
	    menu_font=WMMenuCreate(app, "Fonts");
	    menu_font_color=WMMenuCreate(app, "Colors");
	    /* since shade never work with pixmap .. then it
	     * will work only if transparent is on */
            if(Options & Opt_shade && Options & Opt_transparent)
	    	WMMenuAddSubmenu(menu, "Shades",menu_shade);
	    else
	    	WMMenuAddSubmenu(menu, "BG Colors",menu_background);

	    WMMenuAddSubmenu(menu, "Fonts",menu_font);


	    WMMenuAddItem(menu, "Clone", (WMMenuAction)callmenu_clone, argv,NULL,NULL);



	    WMMenuAddItem(menu_shade, "White", (WMMenuAction)callmenu_changecolor, (void *)Color_White,NULL,NULL);
	    WMMenuAddItem(menu_shade, "Red", (WMMenuAction)callmenu_changecolor, (void *)Color_Red,NULL,NULL);
	    WMMenuAddItem(menu_shade, "Green", (WMMenuAction)callmenu_changecolor, (void *)Color_Green,NULL,NULL);
	    WMMenuAddItem(menu_shade, "Blue", (WMMenuAction)callmenu_changecolor, (void *)Color_Blue,NULL,NULL);
	    WMMenuAddItem(menu_shade, "Cyan", (WMMenuAction)callmenu_changecolor, (void *)Color_Cyan,NULL,NULL);
	    WMMenuAddItem(menu_shade, "Magenta", (WMMenuAction)callmenu_changecolor, (void *)Color_Magenta,NULL,NULL);
	    WMMenuAddItem(menu_shade, "Yellow", (WMMenuAction)callmenu_changecolor, (void *)Color_Yellow,NULL,NULL);
	    WMMenuAddItem(menu_shade, "Black", (WMMenuAction)callmenu_changecolor, (void *)Color_Black,NULL,NULL);
	    WMMenuAddSubmenu(menu_shade, "Select Type",menu_shade_policy);
	    WMMenuAddItem(menu_shade_policy, "Darker", (WMMenuAction)callmenu_policy, (void *)GXand,NULL,NULL);
	    WMMenuAddItem(menu_shade_policy, "Lighter", (WMMenuAction)callmenu_policy, (void *)GXor,NULL,NULL);
	    WMMenuAddItem(menu_shade_policy, "Reverse", (WMMenuAction)callmenu_policy, (void *)GXxor,NULL,NULL);
	    WMMenuAddSubmenu(menu_shade_policy, "Advance...",menu_shade_policy_advance);
	    WMMenuAddItem(menu_shade_policy_advance, "Xclear", (WMMenuAction)callmenu_policy, (void *)GXclear,NULL,NULL);
	    WMMenuAddItem(menu_shade_policy_advance, "Xand", (WMMenuAction)callmenu_policy, (void *)GXand,NULL,NULL);
	    WMMenuAddItem(menu_shade_policy_advance, "XandReverse", (WMMenuAction)callmenu_policy, (void *)GXandReverse,NULL,NULL);
	    WMMenuAddItem(menu_shade_policy_advance, "Xcopy", (WMMenuAction)callmenu_policy, (void *)GXcopy,NULL,NULL);
	    WMMenuAddItem(menu_shade_policy_advance, "XandInverted", (WMMenuAction)callmenu_policy, (void *)GXandInverted,NULL,NULL);
	    WMMenuAddItem(menu_shade_policy_advance, "Xnoop", (WMMenuAction)callmenu_policy, (void *)GXnoop,NULL,NULL);
	    WMMenuAddItem(menu_shade_policy_advance, "Xxor", (WMMenuAction)callmenu_policy, (void *)GXxor,NULL,NULL);
	    WMMenuAddItem(menu_shade_policy_advance, "Xor", (WMMenuAction)callmenu_policy, (void *)GXor,NULL,NULL);
	    WMMenuAddItem(menu_shade_policy_advance, "Xnor", (WMMenuAction)callmenu_policy, (void *)GXnor,NULL,NULL);
	    WMMenuAddItem(menu_shade_policy_advance, "Xequiv", (WMMenuAction)callmenu_policy, (void *)GXequiv,NULL,NULL);
	    WMMenuAddItem(menu_shade_policy_advance, "Xinvert", (WMMenuAction)callmenu_policy, (void *)GXinvert,NULL,NULL);
	    WMMenuAddItem(menu_shade_policy_advance, "XorReverse", (WMMenuAction)callmenu_policy, (void *)GXorReverse,NULL,NULL);
	    WMMenuAddItem(menu_shade_policy_advance, "XcopyInverted", (WMMenuAction)callmenu_policy, (void *)GXcopyInverted,NULL,NULL);
	    WMMenuAddItem(menu_shade_policy_advance, "XorInverted", (WMMenuAction)callmenu_policy, (void *)GXorInverted,NULL,NULL);
	    WMMenuAddItem(menu_shade_policy_advance, "Xnand", (WMMenuAction)callmenu_policy, (void *)GXnand,NULL,NULL);
	    WMMenuAddItem(menu_shade_policy_advance, "Xset", (WMMenuAction)callmenu_policy, (void *)GXset,NULL,NULL);
	    /*******Font***********/
	    WMMenuAddItem(menu_font, "Larger", (WMMenuAction)callmenu_changefont, (void *)1,NULL,NULL);
	    WMMenuAddItem(menu_font, "Smaller", (WMMenuAction)callmenu_changefont, (void *)2,NULL,NULL);
	    /*
	    WMMenuAddItem(menu_font, "User", (WMMenuAction)callmenu_changefont, (void *)3,NULL,NULL);
	    */
	    WMMenuAddSubmenu(menu_font, "Choose Color",menu_font_color);
	    WMMenuAddItem(menu_font, "Restore Color", (WMMenuAction)callmenu_restorecolor, NULL,NULL,NULL);
	    /********background**************/
	    WMMenuAddItem(menu_background, "Swap FG/BG Color", (WMMenuAction)callmenu_swapcolor, (void *)3,NULL,NULL);
	    WMMenuAddItem(menu_background, "White", (WMMenuAction)callmenu_changebackgroundcolor, (void *)Color_White,NULL,NULL);
	    WMMenuAddItem(menu_background, "Red", (WMMenuAction)callmenu_changebackgroundcolor, (void *)Color_Red,NULL,NULL);
	    WMMenuAddItem(menu_background, "Green", (WMMenuAction)callmenu_changebackgroundcolor, (void *)Color_Green,NULL,NULL);
	    WMMenuAddItem(menu_background, "Blue", (WMMenuAction)callmenu_changebackgroundcolor, (void *)Color_Blue,NULL,NULL);
	    WMMenuAddItem(menu_background, "Cyan", (WMMenuAction)callmenu_changebackgroundcolor, (void *)Color_Cyan,NULL,NULL);
	    WMMenuAddItem(menu_background, "Magenta", (WMMenuAction)callmenu_changebackgroundcolor, (void *)Color_Magenta,NULL,NULL);
	    WMMenuAddItem(menu_background, "Yellow", (WMMenuAction)callmenu_changebackgroundcolor, (void *)Color_Yellow,NULL,NULL);
	    WMMenuAddItem(menu_background, "Black", (WMMenuAction)callmenu_changebackgroundcolor, (void *)Color_Black,NULL,NULL);
	    WMMenuAddItem(menu_background, "Dark Red", (WMMenuAction)callmenu_changebackgroundcolor, (void *)Color_Red3,NULL,NULL);
	    WMMenuAddItem(menu_background, "Dark Green", (WMMenuAction)callmenu_changebackgroundcolor, (void *)Color_Green3,NULL,NULL);
	    WMMenuAddItem(menu_background, "Dark Blue", (WMMenuAction)callmenu_changebackgroundcolor, (void *)Color_Blue3,NULL,NULL);
	    WMMenuAddItem(menu_background, "Dark Cyan", (WMMenuAction)callmenu_changebackgroundcolor, (void *)Color_Cyan3,NULL,NULL);
	    WMMenuAddItem(menu_background, "Dark Magenta", (WMMenuAction)callmenu_changebackgroundcolor, (void *)Color_Magenta3,NULL,NULL);
	    WMMenuAddItem(menu_background, "Dark Yellow", (WMMenuAction)callmenu_changebackgroundcolor, (void *)Color_Yellow3,NULL,NULL);
	    /**********************/
	    WMMenuAddItem(menu_font_color, "White", (WMMenuAction)callmenu_changefontcolor, (void *)Color_White,NULL,NULL);
	    WMMenuAddItem(menu_font_color, "Red", (WMMenuAction)callmenu_changefontcolor, (void *)Color_Red,NULL,NULL);
	    WMMenuAddItem(menu_font_color, "Green", (WMMenuAction)callmenu_changefontcolor, (void *)Color_Green,NULL,NULL);
	    WMMenuAddItem(menu_font_color, "Blue", (WMMenuAction)callmenu_changefontcolor, (void *)Color_Blue,NULL,NULL);
	    WMMenuAddItem(menu_font_color, "Cyan", (WMMenuAction)callmenu_changefontcolor, (void *)Color_Cyan,NULL,NULL);
	    WMMenuAddItem(menu_font_color, "Magenta", (WMMenuAction)callmenu_changefontcolor, (void *)Color_Magenta,NULL,NULL);
	    WMMenuAddItem(menu_font_color, "Yellow", (WMMenuAction)callmenu_changefontcolor, (void *)Color_Yellow,NULL,NULL);
	    WMMenuAddItem(menu_font_color, "Black", (WMMenuAction)callmenu_changefontcolor, (void *)Color_Black,NULL,NULL);
	    WMMenuAddItem(menu_font_color, "Dark Red", (WMMenuAction)callmenu_changefontcolor, (void *)Color_Red3,NULL,NULL);
	    WMMenuAddItem(menu_font_color, "Dark Green", (WMMenuAction)callmenu_changefontcolor, (void *)Color_Green3,NULL,NULL);
	    WMMenuAddItem(menu_font_color, "Dark Blue", (WMMenuAction)callmenu_changefontcolor, (void *)Color_Blue3,NULL,NULL);
	    WMMenuAddItem(menu_font_color, "Dark Cyan", (WMMenuAction)callmenu_changefontcolor, (void *)Color_Cyan3,NULL,NULL);
	    WMMenuAddItem(menu_font_color, "Dark Magenta", (WMMenuAction)callmenu_changefontcolor, (void *)Color_Magenta3,NULL,NULL);
	    WMMenuAddItem(menu_font_color, "Dark Yellow", (WMMenuAction)callmenu_changefontcolor, (void *)Color_Yellow3,NULL,NULL);

	    WMMenuAddItem(menu, "Exit", (WMMenuAction)callmenu_exit, NULL,NULL,NULL);
	    WMAppSetMainMenu(app,menu);
	    WMRealizeMenus(app);
    }


/* scrollBar: size doesn't matter */
    scrollBar.win = XCreateSimpleWindow(Xdisplay, TermWin.parent,
					0, 0,
					1, 1,
					0,
					PixColors[Color_fg],
					PixColors[Color_bg]);

    XDefineCursor(Xdisplay, scrollBar.win, cursor);
    XSelectInput(Xdisplay, scrollBar.win,
		 (ExposureMask | ButtonPressMask | ButtonReleaseMask |
		  Button1MotionMask | Button2MotionMask | Button3MotionMask)
	);

    { /* ONLYIF MENUBAR */
	create_menuBar(cursor);
    }

    if ((rs_backgroundPixmap != NULL) && !(Options & Opt_transparent)) {
	char           *p = (char *)rs_backgroundPixmap;

	if ((p = strchr(p, ';')) != NULL) {
	    p++;
	    scale_pixmap(p);
	}
	set_bgPixmap(rs_backgroundPixmap);
    }
# ifdef XPM_BUFFERING
    else
	set_bgPixmap("");
# endif

/* graphics context for the vt window */
    {
	XGCValues       gcvalue;

	gcvalue.font = TermWin.font->fid;
	gcvalue.foreground = PixColors[Color_fg];
	gcvalue.background = PixColors[Color_bg];
	gcvalue.graphics_exposures = 0;
	TermWin.gc = XCreateGC(Xdisplay, TermWin.vt,
			       GCForeground | GCBackground |
			       GCFont | GCGraphicsExposures,
			       &gcvalue);
#ifdef TRANSPARENT
        gcvalue.foreground = PixColors[Color_bg];
        gcvalue.background = PixColors[Color_fg];
	if(rs_gcfunction)
		gcvalue.function = atoi(rs_gcfunction)&0xf;
	else gcvalue.function = GXand;
        TermWin.stippleGC = XCreateGC(Xdisplay, TermWin.vt,
                GCForeground|GCBackground|GCGraphicsExposures|
                GCFunction, &gcvalue);

        gcvalue.foreground = PixColors[Color_fg];
        gcvalue.background = PixColors[Color_bg];
	if(rs_gcfunction)
		gcvalue.function = atoi(rs_gcfunction)&0xf;
	else gcvalue.function = GXor;
        TermWin.stippleGCfg = XCreateGC(Xdisplay, TermWin.vt,
                GCForeground|GCBackground|GCGraphicsExposures|
                GCFunction, &gcvalue);

	if(Options & Opt_transparent && Options & Opt_reverseVideo)
            callmenu_policy((void *)GXxor, 0, 0);
	else if(Options & Opt_transparent && Options & Opt_hilight)
            callmenu_policy((void *)GXor, 0, 0);
#endif
	initfg=PixColors[Color_fg];
	initbg=PixColors[Color_bg];
    }
}
/*}}} */
/*{{{ window resizing - assuming the parent window is the correct size */
/* PROTO */
void
resize_subwindows(int width, int height)
{
    int             x = 0, y = 0;
    int             old_width = TermWin.width;
    int             old_height = TermWin.height;

    TermWin.width = TermWin.ncol * TermWin.fwidth;
    TermWin.height = TermWin.nrow * TermWin.fheight;

/* size and placement */
    if (scrollbar_visible()) {
	scrollBar.beg = 0;
	scrollBar.end = height;
#ifndef XTERM_SCROLLBAR
# ifdef NEXT_SCROLLBAR
    /* arrows are as high as wide - leave 1 pixel gap */
	scrollBar.end -= ((SB_WIDTH-1) * 2);
# else
    /* arrows are as high as wide - leave 1 pixel gap */
	scrollBar.beg += (SB_WIDTH + 1) + sb_shadow;
	scrollBar.end -= (SB_WIDTH + 1) + sb_shadow;
# endif
#endif

	width -= (SB_WIDTH + 2 * sb_shadow);
	if (Options & Opt_scrollBar_right)
	    XMoveResizeWindow(Xdisplay, scrollBar.win, width, 0,
			      (SB_WIDTH + 2 * sb_shadow), height);
	else {
	    XMoveResizeWindow(Xdisplay, scrollBar.win, 0, 0,
			      (SB_WIDTH + 2 * sb_shadow), height);
	    x = (SB_WIDTH + 2 * sb_shadow);	/* placement of vt window */
	}
    }
    { /* ONLYIF MENUBAR */
	if (menubar_visible()) {
	    y = menuBar_TotalHeight();	/* for placement of vt window */
	    Resize_menuBar(x, 0, width, y);
	}
    }
    XMoveResizeWindow(Xdisplay, TermWin.vt, x, y, width, height + 1);

    if (old_width)
	Gr_Resize(old_width, old_height);

    scr_clear();
    resize_pixmap();
    XSync(Xdisplay, 0);
}

/* PROTO */
void
resize(void)
{
    szHint.base_width = (2 * TermWin_internalBorder);
    szHint.base_height = (2 * TermWin_internalBorder);

    szHint.base_width += (scrollbar_visible() ? (SB_WIDTH + 2 * sb_shadow) : 0);
    { /* ONLYIF MENUBAR */
	szHint.base_height += (menubar_visible() ? menuBar_TotalHeight() : 0);
    }

    szHint.min_width = szHint.base_width + szHint.width_inc;
    szHint.min_height = szHint.base_height + szHint.height_inc;

    szHint.width = szHint.base_width + TermWin.width;
    szHint.height = szHint.base_height + TermWin.height;

    szHint.flags = PMinSize | PResizeInc | PBaseSize | PWinGravity;

    XSetWMNormalHints(Xdisplay, TermWin.parent, &szHint);
    XResizeWindow(Xdisplay, TermWin.parent, szHint.width, szHint.height);

    resize_subwindows(szHint.width, szHint.height);
}

/*
 * Redraw window after exposure or size change
 */
/* PROTO */
void
resize_window1(unsigned int width, unsigned int height)
{
    static short    first_time = 1;
    int             new_ncol = (width - szHint.base_width) / TermWin.fwidth;
    int             new_nrow = (height - szHint.base_height) / TermWin.fheight;

    if (first_time ||
	(new_ncol != TermWin.ncol) ||
	(new_nrow != TermWin.nrow)) {
	int             curr_screen = -1;

    /* scr_reset only works on the primary screen */
	if (!first_time) {	/* this is not the first time thru */
	    selection_clear();
	    curr_screen = scr_change_screen(PRIMARY);
	}
	TermWin.ncol = new_ncol;
	TermWin.nrow = new_nrow;

	resize_subwindows(width, height);
	scr_reset();

	if (curr_screen >= 0)	/* this is not the first time thru */
	    scr_change_screen(curr_screen);
	first_time = 0;
    }
}

/*
 * good for toggling 80/132 columns
 */
/* PROTO */
void
set_width(unsigned short width)
{
    unsigned short  height = TermWin.nrow;

    if (width != TermWin.ncol) {
	width = szHint.base_width + width * TermWin.fwidth;
	height = szHint.base_height + height * TermWin.fheight;

	XResizeWindow(Xdisplay, TermWin.parent, width, height);
	resize_window1(width, height);
    }
}

/*
 * Redraw window after exposure or size change
 */
/* PROTO */
void
resize_window(void)
{
    Window          root;
    XEvent          dummy;
    int             x, y;
    unsigned int    border, depth, width, height;

    while (XCheckTypedWindowEvent(Xdisplay, TermWin.parent,
				  ConfigureNotify, &dummy)) ;
/* do we come from an fontchange? */
    if (font_change_count > 0) {
	font_change_count--;
	return;
    }
    XGetGeometry(Xdisplay, TermWin.parent,
		 &root, &x, &y, &width, &height, &border, &depth);
#ifdef TRANSPARENT
    if (ParentWin1 != None && (Options & Opt_transparent)) {
        int px, py;
        static int opx=0, opy=0;
        unsigned w, h;

        XGetGeometry(Xdisplay, ParentWin1, &root, &px, &py, &w, &h, 
                     &border, &depth);

        if (px != opx || py != opy) {
            scr_clear();
            scr_touch();
            opx = px;
            opy = py;
        }
        scrollbar_show(2);
    }
#endif

/* parent already resized */
    resize_window1(width, height);
}
/*}}} */
/*{{{ xterm sequences - title, iconName, color (exptl) */
#ifdef SMART_WINDOW_TITLE
/* PROTO */
void
set_title(const char *str)
{
    char           *name;

    if (XFetchName(Xdisplay, TermWin.parent, &name))
	name = NULL;
    if (name == NULL || strcmp(name, str))
	XStoreName(Xdisplay, TermWin.parent, str);
    if (name)
	XFree(name);
}
#else
# define set_title(str)	XStoreName (Xdisplay, TermWin.parent, str)
#endif

#ifdef SMART_WINDOW_TITLE
/* PROTO */
void
set_iconName(const char *str)
{
    char           *name;

    if (XGetIconName(Xdisplay, TermWin.parent, &name))
	name = NULL;
    if (name == NULL || strcmp(name, str))
	XSetIconName(Xdisplay, TermWin.parent, str);
    if (name)
	XFree(name);
}
#else
# define set_iconName(str) XSetIconName (Xdisplay, TermWin.parent, str)
#endif

#ifdef XTERM_COLOR_CHANGE
/* PROTO */
void
set_window_color(int idx, const char *color)
{
    const char     *const msg = "can't load color \"%s\"";
    XColor          xcol;
    int             i;

    if (color == NULL || *color == '\0')
	return;

/* handle color aliases */
    if (isdigit(*color)) {
	i = atoi(color);
	if (i >= 8 && i <= 15) {	/* bright colors */
	    i -= 8;
# ifndef NO_BRIGHTCOLOR
	    PixColors[idx] = PixColors[minBrightCOLOR + i];
	    goto Done;
# endif
	}
	if (i >= 0 && i <= 7) {	/* normal colors */
	    PixColors[idx] = PixColors[minCOLOR + i];
	    goto Done;
	}
    }
    if (!XParseColor(Xdisplay, Xcmap, color, &xcol) ||
	!XAllocColor(Xdisplay, Xcmap, &xcol)) {
	print_error(msg, color);
	return;
    }
/* XStoreColor (Xdisplay, Xcmap, XColor*); */

/*
 * FIXME: should free colors here, but no idea how to do it so instead,
 * so just keep gobbling up the colormap
 */
# if 0
    for (i = Color_Black; i <= Color_White; i++)
	if (PixColors[idx] == PixColors[i])
	    break;
    if (i > Color_White) {
    /* fprintf (stderr, "XFreeColors: PixColors [%d] = %lu\n", idx, PixColors [idx]); */
	XFreeColors(Xdisplay, Xcmap, (PixColors + idx), 1,
		    DisplayPlanes(Xdisplay, Xscreen));
    }
# endif

    PixColors[idx] = xcol.pixel;

/* XSetWindowAttributes attr; */
/* Cursor cursor; */
  Done:
    if (idx == Color_bg && !(Options & Opt_transparent))
	XSetWindowBackground(Xdisplay, TermWin.vt, PixColors[Color_bg]);

/* handle Color_BD, scrollbar background, etc. */

    set_colorfgbg();
    {
	XColor          fg, bg;

	fg.pixel = PixColors[Color_pointer];
	XQueryColor(Xdisplay, Xcmap, &fg);
	bg.pixel = PixColors[Color_bg];
	XQueryColor(Xdisplay, Xcmap, &bg);
	XRecolorCursor(Xdisplay, TermWin_cursor, &fg, &bg);
    }
/* the only reasonable way to enforce a clean update */
    scr_poweron();
}
#else
# define set_window_color(idx,color)	((void)0)
#endif				/* XTERM_COLOR_CHANGE */

/*
 * XTerm escape sequences: ESC ] Ps;Pt BEL
 *       0 = change iconName/title
 *       1 = change iconName
 *       2 = change title
 *      46 = change logfile (not implemented)
 *      50 = change font
 *
 * wterm extensions:
 *      10 = menu
 *      20 = bg pixmap
 *      39 = change default fg color
 *      49 = change default bg color
 */
/* PROTO */
void
xterm_seq(int op, const char *str)
{
    int             changed = 0;

    assert(str != NULL);
    switch (op) {
    case XTerm_name:
	set_title(str);		/* drop */
    case XTerm_iconName:
	set_iconName(str);
	break;
    case XTerm_title:
	set_title(str);
	break;
    case XTerm_Menu:
    /*
     * menubar_dispatch() violates the constness of the string,
     * so DON'T do it here
     */
	break;
    case XTerm_Pixmap:
	if (*str != ';') {
	    scale_pixmap("");	/* reset to default scaling */
	    set_bgPixmap(str);	/* change pixmap */
	}
	while ((str = strchr(str, ';')) != NULL) {
	    str++;
	    changed += scale_pixmap(str);
	}
	if (changed) {
	    resize_pixmap();
	    scr_touch();
	}
	break;

    case XTerm_restoreFG:
	set_window_color(Color_fg, str);
	break;
    case XTerm_restoreBG:
	set_window_color(Color_bg, str);
	break;
    case XTerm_logfile:
	break;
    case XTerm_font:
	change_font(0, str);
	break;
    }
}
/*}}} */

/*{{{ change_font() - Switch to a new font */
/*
 * init = 1   - initialize
 *
 * fontname == FONT_UP  - switch to bigger font
 * fontname == FONT_DN  - switch to smaller font
 */
/* PROTO */
void
change_font(int init, const char *fontname)
{
    const char     *const msg = "can't load font \"%s\"";
    XFontStruct    *xfont;
    static char    *newfont[NFONTS];

#ifndef NO_BOLDFONT
    static XFontStruct *boldFont = NULL;

#endif
    static int      fnum = FONT0_IDX;	/* logical font number */
    int             idx = 0;	/* index into rs_font[] */

#if (FONT0_IDX == 0)
# define IDX2FNUM(i)	(i)
# define FNUM2IDX(f)	(f)
#else
# define IDX2FNUM(i)	(i == 0 ? FONT0_IDX : (i <= FONT0_IDX ? (i-1) : i))
# define FNUM2IDX(f)	(f == FONT0_IDX ? 0 : (f < FONT0_IDX  ? (f+1) : f))
#endif
#define FNUM_RANGE(i)	(i <= 0 ? 0 : (i >= NFONTS ? (NFONTS-1) : i))

    if (!init) {
	switch (fontname[0]) {
	case '\0':
	    fnum = FONT0_IDX;
	    fontname = NULL;
	    break;

	/* special (internal) prefix for font commands */
	case FONT_CMD:
	    idx = atoi(fontname + 1);
	    switch (fontname[1]) {
	    case '+':		/* corresponds to FONT_UP */
		fnum += (idx ? idx : 1);
		fnum = FNUM_RANGE(fnum);
		break;

	    case '-':		/* corresponds to FONT_DN */
		fnum += (idx ? idx : -1);
		fnum = FNUM_RANGE(fnum);
		break;

	    default:
		if (fontname[1] != '\0' && !isdigit(fontname[1]))
		    return;
		if (idx < 0 || idx >= (NFONTS))
		    return;
		fnum = IDX2FNUM(idx);
		break;
	    }
	    fontname = NULL;
	    break;

	default:
	    if (fontname != NULL) {
	    /* search for existing fontname */
		for (idx = 0; idx < NFONTS; idx++) {
		    if (!strcmp(rs_font[idx], fontname)) {
			fnum = IDX2FNUM(idx);
			fontname = NULL;
			break;
		    }
		}
	    } else
		return;
	    break;
	}
    /* re-position around the normal font */
	idx = FNUM2IDX(fnum);

	if (fontname != NULL) {
	    char           *name;

	    xfont = XLoadQueryFont(Xdisplay, fontname);
	    if (!xfont)
		return;

	    name = MALLOC(strlen(fontname + 1) * sizeof(char));

	    if (name == NULL) {
		XFreeFont(Xdisplay, xfont);
		return;
	    }
	    STRCPY(name, fontname);
	    if (newfont[idx] != NULL)
		FREE(newfont[idx]);
	    newfont[idx] = name;
	    rs_font[idx] = newfont[idx];
	}
    }
    if (TermWin.font)
	XFreeFont(Xdisplay, TermWin.font);

/* load font or substitute */
    xfont = XLoadQueryFont(Xdisplay, rs_font[idx]);
    if (!xfont) {
	print_error(msg, rs_font[idx]);
	rs_font[idx] = "fixed";
	xfont = XLoadQueryFont(Xdisplay, rs_font[idx]);
	if (!xfont) {
	    print_error(msg, rs_font[idx]);
	    goto Abort;
	}
    }
    TermWin.font = xfont;

#ifndef NO_BOLDFONT
/* fail silently */
    if (init && rs_boldFont != NULL)
	boldFont = XLoadQueryFont(Xdisplay, rs_boldFont);
#endif

#ifdef MULTICHAR_SET
    if (TermWin.mfont)
	XFreeFont(Xdisplay, TermWin.mfont);

/* load font or substitute */
    xfont = XLoadQueryFont(Xdisplay, rs_mfont[idx]);
    if (!xfont) {
	print_error(msg, rs_mfont[idx]);
	rs_mfont[idx] = "k14";
	xfont = XLoadQueryFont(Xdisplay, rs_mfont[idx]);
	if (!xfont) {
	    print_error(msg, rs_mfont[idx]);
	    goto Abort;
	}
    }
    TermWin.mfont = xfont;
#endif				/* MULTICHAR_SET */

/* alter existing GC */
    if (!init) {
	XSetFont(Xdisplay, TermWin.gc, TermWin.font->fid);
	menubar_expose();
    }
/* set the sizes */
    {
	int             i, cw, fh, fw = 0;

	fw = TermWin.font->min_bounds.width;
	fh = TermWin.font->ascent + TermWin.font->descent;

	if (TermWin.font->min_bounds.width == TermWin.font->max_bounds.width)
	    TermWin.fprop = 0;	/* Mono-spaced (fixed width) font */
	else
	    TermWin.fprop = 1;	/* Proportional font */

	if (TermWin.fprop == 1)	/* also, if == 0, per_char[] might be NULL */
	    for (i = TermWin.font->min_char_or_byte2;
		 i <= TermWin.font->max_char_or_byte2; i++) {
		cw = TermWin.font->per_char[i].width;
		MIN_IT(cw,TermWin.font->max_bounds.width);
		MAX_IT(fw,cw);
	    }
    /* not the first time thru and sizes haven't changed */
	if (fw == TermWin.fwidth && fh == TermWin.fheight)
	    return;		/* TODO: not return; check MULTI if needed */

	TermWin.fwidth = fw;
	TermWin.fheight = fh;
    }

/* check that size of boldFont is okay */
#ifndef NO_BOLDFONT
    TermWin.boldFont = NULL;
    if (boldFont != NULL) {
	int             i, cw, fh, fw = 0;

	fw = boldFont->min_bounds.width;
	fh = boldFont->ascent + boldFont->descent;
	if (TermWin.fprop == 0) {	/* bold font must also be monospaced */
	    if (fw != boldFont->max_bounds.width)
		fw = -1;
	} else
	    for (i = TermWin.font->min_char_or_byte2;
		 i <= TermWin.font->max_char_or_byte2; i++) {
		cw = boldFont->per_char[i].width;
		MAX_IT(fw, cw);
	    }
	if (fw == TermWin.fwidth && fh == TermWin.fheight)
	    TermWin.boldFont = boldFont;
    }
#endif				/* NO_BOLDFONT */

/* TODO: check that size of Kanji font is okay */

    set_colorfgbg();

    TermWin.width = TermWin.ncol * TermWin.fwidth;
    TermWin.height = TermWin.nrow * TermWin.fheight;

    szHint.width_inc = TermWin.fwidth;
    szHint.height_inc = TermWin.fheight;

    szHint.min_width = szHint.base_width + szHint.width_inc;
    szHint.min_height = szHint.base_height + szHint.height_inc;

    szHint.width = szHint.base_width + TermWin.width;
    szHint.height = szHint.base_height + TermWin.height;
    { /* ONLYIF MENUBAR */
	szHint.height += (delay_menu_drawing ? menuBar_TotalHeight() : 0);
    }

    szHint.flags = PMinSize | PResizeInc | PBaseSize | PWinGravity;

    if (!init) {
	font_change_count++;
	resize();
    }
    return;
  Abort:
    print_error("aborting");	/* fatal problem */
    exit(EXIT_FAILURE);
#undef IDX2FNUM
#undef FNUM2IDX
#undef FNUM_RANGE
}
/*}}} */

/*{{{ main() */
/* PROTO */
int
main(int argc, char *argv[])
{
    int             i;
    char           *val, **cmd_argv = NULL;

/* "WINDOWID=\0" = 10 chars, UINT_MAX = 10 chars */
    static char     windowid_string[20], *display_string, *term_string;

/* Hops - save original arglist for wdw property WM_COMMAND */
    int             saved_argc = argc;
    char          **saved_argv = (char **)MALLOC((argc + 1) * sizeof(char *));


    for (i = 0; i < argc; i++)
	saved_argv[i] = argv[i];
    saved_argv[i] = NULL;	/* NULL terminate that sucker. */
    for (i = 0; i < argc; i++) {
	if (!strcmp(argv[i], "-e") || !strcmp(argv[i], "-exec")) {
	    argc = i;
	    argv[argc] = NULL;
	    if (argv[argc + 1] != NULL)
		cmd_argv = (argv + argc + 1);
	    break;
	}
    }

/*
 * Save and then give up any super-user privileges
 * If we need privileges in any area then we must specifically request it.
 * We should only need to be root in these cases:
 *  1.  write utmp entries on some systems
 *  2.  chown tty on some systems
 */
    privileges(SAVE);
    privileges(IGNORE);

    Options = (Opt_scrollBar);
    Xdisplay = NULL;
    display_name = NULL;
    rs_term_name = NULL;
    rs_cutchars = NULL;
#ifndef NO_BOLDFONT
    rs_boldFont = NULL;
#endif
#ifdef PRINTPIPE
    rs_print_pipe = NULL;
#endif
    rs_title = NULL;		/* title name for window */
    rs_iconName = NULL;		/* icon name for window */
    rs_geometry = NULL;		/* window geometry */
    rs_saveLines = NULL;	/* scrollback buffer [lines] */
    rs_modifier = NULL;	/* modifier */
#if defined (HOTKEY_CTRL) || defined (HOTKEY_META)
/* recognized when combined with HOTKEY */
    ks_bigfont = XK_greater;
    ks_smallfont = XK_less;
#endif

    rs_menu = NULL;
    rs_path = NULL;
    rs_backgroundPixmap = NULL;
#ifndef NO_BACKSPACE_KEY
    rs_backspace_key = NULL;
#endif
#ifndef NO_DELETE_KEY
    rs_delete_key = NULL;
#endif
#ifndef NO_BRIGHTCOLOR
    colorfgbg = DEFAULT_RSTYLE;
#endif
    rs_gcfunction = NULL;

    rs_name = my_basename(argv[0]);
    if (cmd_argv != NULL && cmd_argv[0] != NULL)
	rs_iconName = rs_title = my_basename(cmd_argv[0]);

/*
 * Open display, get options/resources and create the window
 */
    if ((display_name = getenv("DISPLAY")) == NULL)
	display_name = ":0";

    get_options(argc, argv);

    Xdisplay = XOpenDisplay(display_name);
    if (!Xdisplay) {
	print_error("can't open display %s", display_name);
	exit(EXIT_FAILURE);
    }
    extract_resources(Xdisplay, rs_name);

#if defined(XTERM_SCROLLBAR) || defined(NEXT_SCROLLBAR)
    sb_shadow = 0;
#else
    sb_shadow = (Options & Opt_scrollBar_floating) ? 0 : SHADOW;
#endif
    
/*
 * set any defaults not already set
 */
    if (!rs_title)
	rs_title = rs_name;
    if (!rs_iconName)
	rs_iconName = rs_title;
    if (!rs_saveLines || (TermWin.saveLines = atoi(rs_saveLines)) < 0)
	TermWin.saveLines = SAVELINES;

/* no point having a scrollbar without having any scrollback! */
    if (!TermWin.saveLines)
	Options &= ~Opt_scrollBar;

#ifdef PRINTPIPE
    if (!rs_print_pipe)
	rs_print_pipe = PRINTPIPE;
#endif
    if (!rs_cutchars)
	rs_cutchars = CUTCHARS;
#ifndef NO_BACKSPACE_KEY
    if (!rs_backspace_key)
# ifdef DEFAULT_BACKSPACE
	rs_backspace_key = DEFAULT_BACKSPACE;
# else
	rs_backspace_key = "DEC";	/* can toggle between \033 or \177 */
# endif
    else
	(void) Str_escaped((char *)rs_backspace_key);
#endif
#ifndef NO_DELETE_KEY
    if (!rs_delete_key)
# ifdef DEFAULT_DELETE
	rs_delete_key = DEFAULT_DELETE;
# else
	rs_delete_key = "\033[3~";
# endif
    else
	(void) Str_escaped((char *)rs_delete_key);
#endif
#ifndef NO_BOLDFONT
    if (rs_font[0] == NULL && rs_boldFont != NULL) {
	rs_font[0] = rs_boldFont;
	rs_boldFont = NULL;
    }
#endif
    for (i = 0; i < NFONTS; i++) {
	if (!rs_font[i])
	    rs_font[i] = def_fontName[i];
#ifdef MULTICHAR_SET
	if (!rs_mfont[i])
	    rs_mfont[i] = def_mfontName[i];
#endif
    }

#ifdef XTERM_REVERSE_VIDEO
/* this is how xterm implements reverseVideo */
    if (Options & Opt_reverseVideo) {
	if (!rs_color[Color_fg])
	    rs_color[Color_fg] = def_colorName[Color_bg];
	if (!rs_color[Color_bg])
	    rs_color[Color_bg] = def_colorName[Color_fg];
    }
#endif

    for (i = 0; i < NRS_COLORS; i++)
	if (!rs_color[i])
	    rs_color[i] = def_colorName[i];

#ifndef XTERM_REVERSE_VIDEO
/* this is how we implement reverseVideo */
    if (Options & Opt_reverseVideo) {
	const char     *name;

    /* swap foreground/background colors */

	name = rs_color[Color_fg];
	rs_color[Color_fg] = rs_color[Color_bg];
	rs_color[Color_bg] = name;

	name = def_colorName[Color_fg];
	def_colorName[Color_fg] = def_colorName[Color_bg];
	def_colorName[Color_bg] = name;
    }
#endif

/* convenient aliases for setting fg/bg to colors */
    color_aliases(Color_fg);
    color_aliases(Color_bg);
#ifndef NO_CURSORCOLOR
    color_aliases(Color_cursor);
    color_aliases(Color_cursor2);
#endif				/* NO_CURSORCOLOR */
    color_aliases(Color_pointer);
    color_aliases(Color_border);
#ifndef NO_BOLDUNDERLINE
    color_aliases(Color_BD);
    color_aliases(Color_UL);
#endif				/* NO_BOLDUNDERLINE */

#ifdef PREFER_24BIT
    Xdepth = DefaultDepth(Xdisplay, Xscreen);
    Xcmap = DefaultColormap(Xdisplay, Xscreen);
    Xvisual = DefaultVisual(Xdisplay, Xscreen);

/*
 * If depth is not 24, look for a 24bit visual.
 */
    if (Xdepth != 24) {
	XVisualInfo     vinfo;

	if (XMatchVisualInfo(Xdisplay, Xscreen, 24, TrueColor, &vinfo)) {
	    Xdepth = 24;
	    Xvisual = vinfo.visual;
	    Xcmap = XCreateColormap(Xdisplay, RootWindow(Xdisplay, Xscreen),
				    Xvisual, AllocNone);
	}
    }
#endif

/* add startup-menu: */
    { /* ONLYIF MENUBAR */
	delay_menu_drawing = 1;
	menubar_read(rs_menu);
	delay_menu_drawing--;
    }

    Create_Windows(saved_argc, saved_argv);
    scr_reset();		/* initialize screen */
    Gr_reset();			/* reset graphics */

/* add scrollBar, do it directly to avoid resize() */
    scrollbar_mapping(Options & Opt_scrollBar);
/* we can now add menuBar */
    { /* ONLYIF MENUBAR */
	if (delay_menu_drawing) {
	    delay_menu_drawing = 0;
	    menubar_mapping(1);
	}
    }
#if 0
#ifdef DEBUG_X
    XSynchronize(Xdisplay, True);
    XSetErrorHandler((XErrorHandler) abort);
#else
    XSetErrorHandler((XErrorHandler) xerror_handler);
#endif
#endif

#ifdef DISPLAY_IS_IP
/* Fixup display_name for export over pty to any interested terminal
 * clients via "ESC[7n" (e.g. shells).  Note we use the pure IP number
 * (for the first non-loopback interface) that we get from
 * network_display().  This is more "name-resolution-portable", if you
 * will, and probably allows for faster x-client startup if your name
 * server is beyond a slow link or overloaded at client startup.  Of
 * course that only helps the shell's child processes, not us.
 *
 * Giving out the display_name also affords a potential security hole
 */
    val = display_name = network_display(display_name);
    if (val == NULL)
#endif				/* DISPLAY_IS_IP */
	val = XDisplayString(Xdisplay);
    if (display_name == NULL)
	display_name = val;	/* use broken `:0' value */

    i = strlen(val);
    display_string = MALLOC((i + 9) * sizeof(char));

    sprintf(display_string, "DISPLAY=%s", val);
    sprintf(windowid_string, "WINDOWID=%u", (unsigned int)TermWin.parent);

/* add entries to the environment:
 * @ DISPLAY:   in case we started with -display
 * @ WINDOWID:  X window id number of the window
 * @ COLORTERM: terminal sub-name and also indicates its color
 * @ TERM:      terminal name
 */
    putenv(display_string);
    putenv(windowid_string);
/*    FREE(display_string); this cannot be freed */

#ifdef WTERM_TERMINFO
    putenv("TERMINFO=" WTERM_TERMINFO);
#endif

    if (Xdepth <= 2)
	putenv("COLORTERM=" COLORTERMENV "-mono");
    else
	putenv("COLORTERM=" COLORTERMENVFULL);
    if (rs_term_name != NULL) {
	i = strlen(rs_term_name);
	term_string = MALLOC((i + 6) * sizeof(char));

	sprintf(term_string, "TERM=%s", rs_term_name);
	putenv(term_string);
    } else {
	putenv("TERM=" TERMENV);
    }

    init_command(cmd_argv);

    main_loop();		/* main processing loop */
    return EXIT_SUCCESS;
}
/*}}} */
/*----------------------- end-of-file (C source) -----------------------*/
