/* Include prototypes for all files */
/*
 * $Id: protos.h,v 1.3 1998/08/07 12:29:07 mason Exp $
 */
#include "command.pro"
#include "graphics.pro"
#include "grkelot.pro"
#include "main.pro"
#include "menubar.pro"
#include "misc.pro"
#include "netdisp.pro"
#include "rmemset.pro"
#include "screen.pro"
#ifdef NEXT_SCROLLBAR
# include "scrollbar2.pro"
#else
# include "scrollbar.pro"
#endif
#ifdef UTMP_SUPPORT
#include "utmp.pro"
#endif
#include "xdefaults.pro"
#include "xpm.pro"
