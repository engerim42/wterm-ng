/*--------------------------------*-C-*---------------------------------*
 * File:	scrollbar2.c
 *----------------------------------------------------------------------*
 * Copyright (C) 1998 Alfredo K. Kojima <kojima@windowmaker.org>
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
/*
 * Same code as scrollbar.c, but this one is rewritten to do 
 * N*XTSTEP like scrollbars.
 */


#include "wterm.h"		/* NECESSARY */

/*----------------------------------------------------------------------*
 */

static GC blackGC;
static GC whiteGC;
static GC grayGC;
static GC darkGC;
static GC stippleGC;

static Pixmap dimple;
static Pixmap upArrow;
static Pixmap downArrow;
static Pixmap upArrowHi;
static Pixmap downArrowHi;

static char *SCROLLER_DIMPLE[] = {
".%###.",
"%#%%%%",
"#%%...",
"#%..  ",
"#%.   ",
".%.  ."
};

#define SCROLLER_DIMPLE_WIDTH   6
#define SCROLLER_DIMPLE_HEIGHT  6



static char *SCROLLER_ARROW_UP[] = {
".............",
".............",
"......%......",
"......#......",
".....%#%.....",
".....###.....",
"....%###%....",
"....#####....",
"...%#####%...",
"...#######...",
"..%#######%..",
".............",
"............."
};

static char *SCROLLER_ARROW_DOWN[] = {
".............",
".............",
"..%#######%..",
"...#######...",
"...%#####%...",
"....#####....",
"....%###%....",
".....###.....",
".....%#%.....",
"......#......",
"......%......",
".............",
"............."
};


static char *HI_SCROLLER_ARROW_UP[] = {
"             ",
"             ",
"      %      ",
"      %      ",
"     %%%     ",
"     %%%     ",
"    %%%%%    ",
"    %%%%%    ",
"   %%%%%%%   ",
"   %%%%%%%   ",
"  %%%%%%%%%  ",
"             ",
"             "
};

static char *HI_SCROLLER_ARROW_DOWN[] = {
"             ",
"             ",
"  %%%%%%%%%  ",
"   %%%%%%%   ",
"   %%%%%%%   ",
"    %%%%%    ",
"    %%%%%    ",
"     %%%     ",
"     %%%     ",
"      %      ",
"      %      ",
"             ",
"             "
};

#define ARROW_WIDTH   13
#define ARROW_HEIGHT  13


#define stp_width 8
#define stp_height 8
static unsigned char stp_bits[] = {
   0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa};



static Pixmap
renderPixmap(char **data, int width, int height)
{
    int x, y;
    Pixmap d;
    
    d = XCreatePixmap(Xdisplay, scrollBar.win, width, height, Xdepth);

    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            switch (data[y][x]) {               
             case ' ':
             case 'w':
                XDrawPoint(Xdisplay, d, whiteGC, x, y);
                break;

             case '.':
             case 'l':
                XDrawPoint(Xdisplay, d, grayGC, x, y);
                break;
                
             case '%':
             case 'd':
                XDrawPoint(Xdisplay, d, darkGC, x, y);
                break;
                
             case '#':
             case 'b':
             default:
                XDrawPoint(Xdisplay, d, blackGC, x, y);
                break;
            }
        }
    }
    
    return d;
}

#ifdef TRANSPARENT
void scrollbar_getPix(void)
{
    int height = scrollBar.end + ((SB_WIDTH-1) * 2) + sb_shadow;

    if (stippleGC)
      XFreePixmap(Xdisplay, scrollBar.bg_pixmap);

    scrollBar.bg_pixmap = XCreatePixmap(Xdisplay, scrollBar.win, SB_WIDTH+1, height, Xdepth);
    XSetWindowBackgroundPixmap(Xdisplay, scrollBar.win, ParentRelative);
    XClearArea(Xdisplay, scrollBar.win, 0, 0, SB_WIDTH+1, height, 1);
    XCopyArea(Xdisplay, scrollBar.win, scrollBar.bg_pixmap, whiteGC, 0, 0,
                SB_WIDTH+1, height, 0, 0);

}

#endif

static void
init_stuff(void)
{
    XGCValues gcvalue;
    XColor xcol;
    Pixmap stipple;
    unsigned long light, dark;

    gcvalue.graphics_exposures = False;
    
    gcvalue.foreground = BlackPixelOfScreen(DefaultScreenOfDisplay(Xdisplay));
    blackGC = XCreateGC(Xdisplay, scrollBar.win, GCForeground|GCGraphicsExposures,
			&gcvalue);
    
    gcvalue.foreground = WhitePixelOfScreen(DefaultScreenOfDisplay(Xdisplay));
    whiteGC = XCreateGC(Xdisplay, scrollBar.win, GCForeground|GCGraphicsExposures,
			&gcvalue);

    xcol.red = 0xaeba;
    xcol.green = 0xaaaa;
    xcol.blue = 0xaeba;
    if (!XAllocColor (Xdisplay, Xcmap, &xcol))
    {
	print_error ("can't allocate %s", "light gray");
	xcol.pixel = PixColors [Color_AntiqueWhite];
    }
    
    light = gcvalue.foreground = xcol.pixel;
    grayGC =  XCreateGC(Xdisplay, scrollBar.win, GCForeground|GCGraphicsExposures,
			&gcvalue);

    xcol.red = 0x51aa;
    xcol.green = 0x5555;
    xcol.blue = 0x5144;
    if (!XAllocColor (Xdisplay, Xcmap, &xcol))
    {
	print_error ("can't allocate %s", "dark gray");
	xcol.pixel = PixColors [Color_Grey25];
    }

    dark = gcvalue.foreground = xcol.pixel;
    darkGC =  XCreateGC(Xdisplay, scrollBar.win, GCForeground|GCGraphicsExposures,
			&gcvalue);

    stipple = XCreateBitmapFromData(Xdisplay, scrollBar.win,
				    stp_bits, stp_width, stp_height);
    
    gcvalue.foreground = dark;
    gcvalue.background = light;
    gcvalue.fill_style = FillStippled;
    gcvalue.stipple = stipple;
    
/*    XSetWindowBackground(Xdisplay, scrollBar.win, TermWin.pixmap); */
#ifdef TRANSPARENT
    if (Options & Opt_transparent){
      scrollbar_getPix();
    }
#endif

    stippleGC = XCreateGC(Xdisplay, scrollBar.win, GCForeground|GCBackground
			  |GCStipple|GCFillStyle|GCGraphicsExposures, 
			  &gcvalue);

    dimple = renderPixmap(SCROLLER_DIMPLE, SCROLLER_DIMPLE_WIDTH,
			  SCROLLER_DIMPLE_HEIGHT);
    
    upArrow = renderPixmap(SCROLLER_ARROW_UP, ARROW_WIDTH, ARROW_HEIGHT);
    downArrow = renderPixmap(SCROLLER_ARROW_DOWN, ARROW_WIDTH, ARROW_HEIGHT);
    upArrowHi = renderPixmap(HI_SCROLLER_ARROW_UP, ARROW_WIDTH, ARROW_HEIGHT);
    downArrowHi = renderPixmap(HI_SCROLLER_ARROW_DOWN, ARROW_WIDTH, ARROW_HEIGHT);

    scrollbar_show(1);
}


/* Draw bevel & arrows */
static void
drawBevel(Drawable d, int x, int y, int w, int h)
{
    XDrawLine(Xdisplay, d, whiteGC, x, y, x+w-1, y);
    XDrawLine(Xdisplay, d, whiteGC, x, y, x, y+h-1);
    
    XDrawLine(Xdisplay, d, blackGC, x+w-1, y, x+w-1, y+h-1);
    XDrawLine(Xdisplay, d, blackGC, x, y+h-1, x+w-1, y+h-1);
    
    XDrawLine(Xdisplay, d, darkGC, x+1, y+h-2, x+w-2, y+h-2);
    XDrawLine(Xdisplay, d, darkGC, x+w-2, y+1, x+w-2, y+h-2);
}



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


int
scrollbar_show(int update)
{
    /* old (drawn) values */
    static int last_top, last_bot, last_len;
    static int scrollbar_len;		/* length of slider */
    Pixmap buffer;
    int height = scrollBar.end + ((SB_WIDTH-1) * 2) + sb_shadow;
    XGCValues getvalue;

    if (blackGC == NULL)
	init_stuff();

    if (update == 1)
    {
	int             top = (TermWin.nscrolled - TermWin.view_start);
	int             bot = top + (TermWin.nrow - 1);
	int             len = max((TermWin.nscrolled + (TermWin.nrow - 1)),1);

	scrollBar.top = (scrollBar.beg + (top * scrollbar_size()) / len);
	scrollBar.bot = (scrollBar.beg + (bot * scrollbar_size()) / len);
    /* no change */
	if ((scrollBar.top == last_top) && (scrollBar.bot == last_bot))
	    return 0;
	
	scrollbar_len = scrollBar.bot - scrollBar.top;

    }
#ifdef TRANSPARENT
    else if (update == 2)
      if (Options & Opt_transparent)
        scrollbar_getPix();
#endif

    /* create double buffer */
    buffer = XCreatePixmap(Xdisplay, scrollBar.win, SB_WIDTH+1, height, Xdepth);

    /* draw the background */ 
#ifdef TRANSPARENT
    if ((Options & Opt_transparent) && (Options & Opt_scrollBar_floating)){
          XCopyArea(Xdisplay, scrollBar.bg_pixmap, buffer, 
			  whiteGC, 0, 0, SB_WIDTH+1, height, 0, 0);
    }
    else
#endif
    if (Options & Opt_scrollBar_floating)
      XFillRectangle(Xdisplay, buffer, blackGC, 0, 0, SB_WIDTH+1, height);
    else {
    XFillRectangle(Xdisplay, buffer, grayGC, 0, 0, SB_WIDTH+1, height);
    XDrawRectangle(Xdisplay, buffer, blackGC, 0, 0, SB_WIDTH, height);
    }

    if (TermWin.nscrolled > 0) 
    {
        if (!(Options & Opt_scrollBar_floating)) {
	XFillRectangle(Xdisplay, buffer, stippleGC, 2, 1,
		       SB_WIDTH-3, height-2*(SB_WIDTH-1));
        }
#ifdef TRANSPARENT
        else if (Options & Opt_shade) {
          XFillRectangle(Xdisplay, buffer, TermWin.stippleGC, 0, 0,
                         SB_WIDTH+1, height);
        }
#endif
	
	XFillRectangle(Xdisplay, buffer, grayGC, 2, scrollBar.top+1,
		       SB_WIDTH-3, scrollbar_len);

	drawBevel(buffer, 2, scrollBar.top+1, SB_WIDTH-3, scrollbar_len);
	
	drawBevel(buffer, 2, height-2*(SB_WIDTH-2), SB_WIDTH-3, SB_WIDTH-3);
		
	drawBevel(buffer, 2, height-(SB_WIDTH-2), SB_WIDTH-3, SB_WIDTH-3);
	
	XCopyArea(Xdisplay, dimple, buffer, whiteGC, 0, 0, 
		  SCROLLER_DIMPLE_WIDTH, SCROLLER_DIMPLE_HEIGHT,
		  (SB_WIDTH-6)/2, 
		  scrollBar.top + 1 + (scrollbar_len-SCROLLER_DIMPLE_HEIGHT)/2);
	
	if (scrollbar_isUp())
	    XCopyArea(Xdisplay, upArrowHi, buffer, whiteGC, 0, 0, ARROW_WIDTH,
		      ARROW_HEIGHT, 3, height - (SB_WIDTH-1) - 15);
	else
	    XCopyArea(Xdisplay, upArrow, buffer, whiteGC, 0, 0, ARROW_WIDTH,
		      ARROW_HEIGHT, 3, height - (SB_WIDTH-1) - 15);

	if (scrollbar_isDn())
	    XCopyArea(Xdisplay, downArrowHi, buffer, whiteGC, 0, 0, ARROW_WIDTH,
		      ARROW_HEIGHT, 3, height - 16);
	else
	    XCopyArea(Xdisplay, downArrow, buffer, whiteGC, 0, 0, ARROW_WIDTH,
		      ARROW_HEIGHT, 3, height - 16);
    } else {
        if (!(Options & Opt_scrollBar_floating))
	XFillRectangle(Xdisplay, buffer, stippleGC, 2, 1,
		       SB_WIDTH-3, height-2);
#ifdef TRANSPARENT
        else if (Options & Opt_shade) {
          XFillRectangle(Xdisplay, buffer, TermWin.stippleGC, 0, 0,
                         SB_WIDTH+1, height);
        }
#endif
    }
    
        XGetGCValues(Xdisplay, TermWin.stippleGC, GCFunction, &getvalue);
	if (
			(Options & Opt_scrollBar_floating)
			&& (Options & Opt_transparent)){


	    XSetWindowBackgroundPixmap(Xdisplay, scrollBar.win, ParentRelative);
	    /*
	    XClearArea(Xdisplay, scrollBar.win, 0, 0, SB_WIDTH+1, height, 0);
	    */
	    /*
	    XClearArea(Xdisplay, scrollBar.win, 0, 0, SB_WIDTH+1, scrollBar.top, 0);
	    XClearArea(Xdisplay, scrollBar.win, 0, scrollBar.bot, SB_WIDTH+1, 
			  height-2*(SB_WIDTH-1)-scrollBar.bot,0);
			  */
	    /***oh***
	    XClearArea(Xdisplay, scrollBar.win, 0, last_top,
			    SB_WIDTH+1, scrollBar.top-last_top , 0);
		 	    */
	    if (getvalue.function==GXand) 
	    	XClearArea(Xdisplay, scrollBar.win, 0, last_top,
			    SB_WIDTH+1, scrollBar.top-last_top , 0);
	    else
	    	XClearArea(Xdisplay, scrollBar.win, 0, 0, SB_WIDTH+1, height, 0);

	    if(TermWin.nscrolled){
		    XCopyArea(Xdisplay, buffer, scrollBar.win, grayGC, 2, scrollBar.top, 
				  SB_WIDTH-3, scrollbar_len+1, 2, scrollBar.top);
		    XCopyArea(Xdisplay, buffer, scrollBar.win, grayGC, 2, height-2*(SB_WIDTH), 
				  SB_WIDTH-3, 2*(SB_WIDTH), 2, height-2*(SB_WIDTH));
		    XFillRectangle(Xdisplay, scrollBar.win, TermWin.stippleGC, 2, 0,
				  SB_WIDTH-3, scrollBar.top);

		    /*
		    XFillRectangle(Xdisplay, scrollBar.win, TermWin.stippleGC, 2, scrollBar.bot+1,
				  SB_WIDTH-3, height-2*(SB_WIDTH)-(scrollBar.bot+1));
					*/
		    XFillRectangle(Xdisplay, scrollBar.win, TermWin.stippleGC, 2, scrollBar.bot+1,
				  SB_WIDTH-3, (height-2*(SB_WIDTH)-scrollBar.bot)>0
				  	?(height-2*(SB_WIDTH)-(scrollBar.bot+1)):0);
		    XFillRectangle(Xdisplay, scrollBar.win, TermWin.stippleGC, 0, 0,
				  2, height);
		    XFillRectangle(Xdisplay, scrollBar.win, TermWin.stippleGC, SB_WIDTH-1, 0,
				  1, height);
	    }
	    else {
		    XFillRectangle(Xdisplay, scrollBar.win, TermWin.stippleGC, 0, 0,
				  SB_WIDTH+1, height);
	    }

	}
	else {
	    XCopyArea(Xdisplay, buffer, scrollBar.win, grayGC, 0, 0, 
                          SB_WIDTH+1, height, 0, 0);
	}


    XFreePixmap(Xdisplay, buffer);
    last_top = scrollBar.top;
    last_bot = scrollBar.bot;
    last_len = scrollbar_len;


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
