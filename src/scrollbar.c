/*--------------------------------*-C-*---------------------------------*
 * File:	scrollbar.c
 *----------------------------------------------------------------------*
 * Copyright (C) 1997,1998 mj olesen <olesen@me.QueensU.CA>
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

#ifndef lint
static const char rcsid[] = "$Id: scrollbar.c,v 1.7 1998/09/19 12:43:58 mason Exp $";
#endif

#include "wterm.h"		/* NECESSARY */

/*----------------------------------------------------------------------*
 */
static GC       scrollbarGC;

#ifdef XTERM_SCROLLBAR		/* bitmap scrollbar */
static GC	ShadowGC;

static char     sb_bits[] =
{0xaa, 0x0a, 0x55, 0x05};	/* 12x2 bitmap */

#if (SB_WIDTH != 15)
Error, check scrollbar width (SB_WIDTH). It must be 15 for XTERM_SCROLLBAR
#endif

#else				/* XTERM_SCROLLBAR */
static GC       topShadowGC, botShadowGC;

/* draw triangular up button with a shadow of SHADOW (1 or 2) pixels */
/* PROTO */
void
Draw_up_button(int x, int y, int state)
{
    const unsigned int sz = (SB_WIDTH), sz2 = (SB_WIDTH / 2);
    XPoint          pt[3];
    GC              top, bot;

    switch (state) {
    case +1:
	top = topShadowGC;
	bot = botShadowGC;
	break;
    case -1:
	top = botShadowGC;
	bot = topShadowGC;
	break;
    default:
	top = bot = scrollbarGC;
	break;
    }

/* fill triangle */
    pt[0].x = x;
    pt[0].y = y + sz - 1;
    pt[1].x = x + sz - 1;
    pt[1].y = y + sz - 1;
    pt[2].x = x + sz2;
    pt[2].y = y;
    XFillPolygon(Xdisplay, scrollBar.win, scrollbarGC,
		 pt, 3, Convex, CoordModeOrigin);

/* draw base */
    XDrawLine(Xdisplay, scrollBar.win, bot,
	      pt[0].x, pt[0].y, pt[1].x, pt[1].y);

/* draw shadow */
    pt[1].x = x + sz2 - 1;
    pt[1].y = y;
    XDrawLine(Xdisplay, scrollBar.win, top,
	      pt[0].x, pt[0].y, pt[1].x, pt[1].y);
#if (SHADOW > 1)
/* doubled */
    pt[0].x++;
    pt[0].y--;
    pt[1].y++;
    XDrawLine(Xdisplay, scrollBar.win, top,
	      pt[0].x, pt[0].y, pt[1].x, pt[1].y);
#endif
/* draw shadow */
    pt[0].x = x + sz2;
    pt[0].y = y;
    pt[1].x = x + sz - 1;
    pt[1].y = y + sz - 1;
    XDrawLine(Xdisplay, scrollBar.win, bot,
	      pt[0].x, pt[0].y, pt[1].x, pt[1].y);
#if (SHADOW > 1)
/* doubled */
    pt[0].y++;
    pt[1].x--;
    pt[1].y--;
    XDrawLine(Xdisplay, scrollBar.win, bot,
	      pt[0].x, pt[0].y, pt[1].x, pt[1].y);
#endif
}

/* draw triangular down button with a shadow of SHADOW (1 or 2) pixels */
/* PROTO */
void
Draw_dn_button(int x, int y, int state)
{
    const unsigned int sz = (SB_WIDTH), sz2 = (SB_WIDTH / 2);
    XPoint          pt[3];
    GC              top, bot;

    switch (state) {
    case +1:
	top = topShadowGC;
	bot = botShadowGC;
	break;
    case -1:
	top = botShadowGC;
	bot = topShadowGC;
	break;
    default:
	top = bot = scrollbarGC;
	break;
    }

/* fill triangle */
    pt[0].x = x;
    pt[0].y = y;
    pt[1].x = x + sz - 1;
    pt[1].y = y;
    pt[2].x = x + sz2;
    pt[2].y = y + sz;
    XFillPolygon(Xdisplay, scrollBar.win, scrollbarGC,
		 pt, 3, Convex, CoordModeOrigin);

/* draw base */
    XDrawLine(Xdisplay, scrollBar.win, top,
	      pt[0].x, pt[0].y, pt[1].x, pt[1].y);

/* draw shadow */
    pt[1].x = x + sz2 - 1;
    pt[1].y = y + sz - 1;
    XDrawLine(Xdisplay, scrollBar.win, top,
	      pt[0].x, pt[0].y, pt[1].x, pt[1].y);
#if (SHADOW > 1)
/* doubled */
    pt[0].x++;
    pt[0].y++;
    pt[1].y--;
    XDrawLine(Xdisplay, scrollBar.win, top,
	      pt[0].x, pt[0].y, pt[1].x, pt[1].y);
#endif
/* draw shadow */
    pt[0].x = x + sz2;
    pt[0].y = y + sz - 1;
    pt[1].x = x + sz - 1;
    pt[1].y = y;
    XDrawLine(Xdisplay, scrollBar.win, bot,
	      pt[0].x, pt[0].y, pt[1].x, pt[1].y);
#if (SHADOW > 1)
/* doubled */
    pt[0].y--;
    pt[1].x--;
    pt[1].y++;
    XDrawLine(Xdisplay, scrollBar.win, bot,
	      pt[0].x, pt[0].y, pt[1].x, pt[1].y);
#endif
}
#endif				/* XTERM_SCROLLBAR */

/* PROTO */
int
scrollbar_mapping(int map)
{
    int             change = 0;

    if (map && !scrollbar_visible()) {
	scrollBar.state = 1;
	XMapWindow(Xdisplay, scrollBar.win);
	change = 1;
    } else if (!map && scrollbar_visible()) {
	scrollBar.state = 0;
	XUnmapWindow(Xdisplay, scrollBar.win);
	change = 1;
    }
    return change;
}

/* PROTO */
int
scrollbar_show(int update)
{
    static short    last_top, last_bot, sb_width;	/* old (drawn) values */
    int		    xsb = 0;

    if (!scrollbar_visible())
	return 0;

    if (scrollbarGC == None) {
	XGCValues       gcvalue;

#ifdef XTERM_SCROLLBAR
	sb_width = SB_WIDTH - 1;
	gcvalue.stipple = XCreateBitmapFromData(Xdisplay, scrollBar.win,
						sb_bits, 12, 2);
	if (!gcvalue.stipple) {
	    print_error("can't create bitmap");
	    exit(EXIT_FAILURE);
	}
	gcvalue.fill_style = FillOpaqueStippled;
	gcvalue.foreground = PixColors[Color_fg];
	gcvalue.background = PixColors[Color_bg];

	scrollbarGC = XCreateGC(Xdisplay, scrollBar.win,
				GCForeground | GCBackground |
				GCFillStyle | GCStipple,
				&gcvalue);
        gcvalue.foreground = PixColors[Color_border];
	ShadowGC = XCreateGC(Xdisplay, scrollBar.win, GCForeground, &gcvalue);
#else				/* XTERM_SCROLLBAR */
	sb_width = SB_WIDTH;

	gcvalue.foreground = PixColors[Color_trough];
	if (sb_shadow) {
	    XSetWindowBackground(Xdisplay, scrollBar.win, gcvalue.foreground);
	    XClearWindow(Xdisplay, scrollBar.win);
	}

	gcvalue.foreground = (Xdepth <= 2 ? PixColors[Color_fg]
					  : PixColors[Color_scroll]);
	scrollbarGC = XCreateGC(Xdisplay, scrollBar.win, GCForeground,
				&gcvalue);

	gcvalue.foreground = PixColors[Color_topShadow];
	topShadowGC = XCreateGC(Xdisplay, scrollBar.win,
				GCForeground,
				&gcvalue);

	gcvalue.foreground = PixColors[Color_bottomShadow];
	botShadowGC = XCreateGC(Xdisplay, scrollBar.win,
				GCForeground,
				&gcvalue);
#endif				/* XTERM_SCROLLBAR */
    }

    if (update == 1) {
	int             top = (TermWin.nscrolled - TermWin.view_start);
	int             bot = top + (TermWin.nrow - 1);
	int             len = max((TermWin.nscrolled + (TermWin.nrow - 1)),1);

	scrollBar.top = (scrollBar.beg + (top * scrollbar_size()) / len);
	scrollBar.bot = (scrollBar.beg + (bot * scrollbar_size()) / len);
    /* no change */
	if ((scrollBar.top == last_top) && (scrollBar.bot == last_bot))
	    return 0;
    }
/* instead of XClearWindow (Xdisplay, scrollBar.win); */
#ifdef XTERM_SCROLLBAR
    xsb = (Options & Opt_scrollBar_right) ? 1 : 0;
#endif

#ifdef TRANSPARENT
    if ((Options & Opt_transparent) && (Options & Opt_scrollBar_floating)) {
      XSetWindowBackgroundPixmap(Xdisplay, scrollBar.win, ParentRelative);
      if (update == 2) {
        XClearWindow(Xdisplay, scrollBar.win);
        if (Options & Opt_shade)
        XFillRectangle(Xdisplay, scrollBar.win, TermWin.stippleGC,
                       0, 0, sb_width, TermWin.height);
      }
    }
#endif

    if (last_top < scrollBar.top) {
	XClearArea(Xdisplay, scrollBar.win,
		   sb_shadow + xsb, last_top,
		   sb_width, (scrollBar.top - last_top),
		   False);
#ifdef TRANSPARENT
        if ((Options & Opt_shade) && (Options & Opt_scrollBar_floating))
        XFillRectangle(Xdisplay, scrollBar.win, TermWin.stippleGC,
                       sb_shadow + xsb, last_top, sb_width, (scrollBar.top - last_top));
#endif
    }

    if (scrollBar.bot < last_bot) {
	XClearArea(Xdisplay, scrollBar.win,
		   sb_shadow + xsb, scrollBar.bot,
		   sb_width, (last_bot - scrollBar.bot),
		   False);
#ifdef TRANSPARENT
        if ((Options & Opt_shade) && (Options & Opt_scrollBar_floating))
        XFillRectangle(Xdisplay, scrollBar.win, TermWin.stippleGC,
                       sb_shadow + xsb, scrollBar.bot,
                       sb_width, (last_bot - scrollBar.bot));
#endif
    }

    last_top = scrollBar.top;
    last_bot = scrollBar.bot;

/* scrollbar slider */
#ifdef XTERM_SCROLLBAR
    XFillRectangle(Xdisplay, scrollBar.win, scrollbarGC,
		   xsb + 1, scrollBar.top,
		   sb_width - 2, (scrollBar.bot - scrollBar.top));

    XDrawLine(Xdisplay, scrollBar.win, ShadowGC,
	      xsb ? 0 : 14, scrollBar.beg, xsb ? 0 : 14, scrollBar.end);
#else
# ifdef SB_BORDER
   XDrawLine(Xdisplay, scrollBar.win, botShadowGC,
	     SB_WIDTH, 0, SB_WIDTH, scrollBar.end + SB_WIDTH) ;
# endif
   XFillRectangle(Xdisplay, scrollBar.win, scrollbarGC,
		   sb_shadow, scrollBar.top,
		   sb_width, (scrollBar.bot - scrollBar.top));

    if (sb_shadow)
    /* trough shadow */
	Draw_Shadow(scrollBar.win,
		    botShadowGC, topShadowGC,
		    0, 0,
		    (sb_width + 2 * sb_shadow),
		    (scrollBar.end + (sb_width + 1) + sb_shadow));
/* shadow for scrollbar slider */
    Draw_Shadow(scrollBar.win,
		topShadowGC, botShadowGC,
		sb_shadow, scrollBar.top, sb_width,
		(scrollBar.bot - scrollBar.top));

/*
 * Redraw scrollbar arrows
 */
    Draw_up_button(sb_shadow, sb_shadow, (scrollbar_isUp()? -1 : +1));
    Draw_dn_button(sb_shadow, (scrollBar.end + 1), (scrollbar_isDn()? -1 : +1));
#endif				/* XTERM_SCROLLBAR */

    return 1;
}

/* PROTO */
void
map_scrollBar(int map)
{
    if (scrollbar_mapping(map)) {
	resize();
	scr_touch();
    }
}

/*----------------------- end-of-file (C source) -----------------------*/
