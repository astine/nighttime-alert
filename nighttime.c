/* See LICENSE file for copyright and license details. */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif
#include "draw.h"

#define INTERSECT(x,y,w,h,r)  (MAX(0, MIN((x)+(w),(r).x_org+(r).width)  - MAX((x),(r).x_org)) \
                             * MAX(0, MIN((y)+(h),(r).y_org+(r).height) - MAX((y),(r).y_org)))
#define MIN(a,b)              ((a) < (b) ? (a) : (b))
#define MAX(a,b)              ((a) > (b) ? (a) : (b))

#define MESSAGE_INTERVAL 180

static void grabkeyboard(void);
static void releasekeyboard(void);
static int checkforescape(XKeyEvent *ev);
static void drawwarning(void);
static void drawshutdown(void);
static void *displaymessage(void *arg);
static void setup(void);

static int bh, mw, mh;
static Atom clip, utf8;
static const char *normbgcolor = "#222222";
static const char *normfgcolor = "#bbbbbb";
static const char *selbgcolor  = "#005577";
static const char *selfgcolor  = "#eeeeee";
static const char *font = NULL;
static unsigned long normcol[ColLast];
static unsigned long selcol[ColLast];
static DC *dc;
static Window win;
static XIC xic;
static int window_open = 0;
static time_t last_window_close = 0;
static struct tm shutdown_tm;

static void (*draw)(void) = drawwarning;
//static int (*fstrncmp)(const char *, const char *, size_t) = strncmp;
//static char *(*fstrstr)(const char *, const char *) = strstr;

int
main(int argc, char *argv[]) {

  int err;
  pthread_t ntid;

  time_t now = time(NULL);
  localtime_r(&now, &shutdown_tm);
  shutdown_tm.tm_hour = 22; //default shutdown hour
  shutdown_tm.tm_min = 30;  //default shutdown minute
  shutdown_tm.tm_sec = 0;   //default shutdown minute

  for(int i=0;i<argc;i++)
    if(!strcmp(argv[i],"-t"))
      {
	char *colon = strchr(argv[++i], ':');
	*colon++ = '\0';
	shutdown_tm.tm_hour = strtol(argv[i],NULL,10);
	shutdown_tm.tm_min = strtol(colon,NULL,10);
      }

  time_t shutdown = mktime(&shutdown_tm);
  if(difftime(now,shutdown) >= 0)
    {
      shutdown = shutdown+86400;
      localtime_r(&shutdown,&shutdown_tm);
    }

  while(difftime(now,shutdown) < 0)
    {
      if (window_open == 0 && last_window_close + MESSAGE_INTERVAL < time(NULL))
	{
	  err = pthread_create(&ntid, NULL, displaymessage, NULL);
	  if (err != 0)
	    eprintf("cannot display message\n");
	}
      sleep(1);
      now = time(NULL);
    }

  draw = drawshutdown;
  if (window_open == 0)
    {
      err = pthread_create(&ntid, NULL, displaymessage, NULL);
      if (err != 0)
	eprintf("cannot display message\n");
    }
  
  sleep(2);
  system("/usr/bin/systemctl poweroff");
  sleep(1);

  return 0; 
}

void *displaymessage(void *arg)
{
  window_open = 1;
  XEvent ev;
  dc = initdc();
  initfont(dc, font);
  grabkeyboard();
  setup();
  
  while(!XNextEvent(dc->dpy, &ev)) {
    if(XFilterEvent(&ev, win))
      continue;
    switch(ev.type) {
    case Expose:
      if(ev.xexpose.count == 0)
	mapdc(dc, win, mw, mh);
      break;
    case KeyPress:
      if(checkforescape(&ev.xkey) && draw != drawshutdown)
	{
	  releasekeyboard();
	  freedc(dc);
	  last_window_close = time(NULL);
	  window_open = 0;
	  return ((void *)0);
	}
      break;
    case SelectionNotify:
      if(ev.xselection.property == utf8);
      break;
    case VisibilityNotify:
      if(ev.xvisibility.state != VisibilityUnobscured)
	XRaiseWindow(dc->dpy, win);
      break;
    }
    draw();
  }
  return ((void *)0);
}

void drawwarning(void) {
  const time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  char string[100]; 
  strftime(string, 100, "It is now %I:%M %p, time for bed.", tm);
  dc->x = 0;
  dc->y = 0;
  drawrect(dc, 0, 0, mw, mh, True, BG(dc, normcol));
  dc->x = mw/2-(dc->font.width*strlen(string))/2;
  dc->y = mh/2-(bh*3/2);
  drawtext(dc, string, normcol);
  strftime(string, 100, "This computer will shutdown at %I:%M %p", &shutdown_tm);
  dc->y = dc->y+bh;
  drawtext(dc, string, normcol);
  dc->y = dc->y+bh;
  drawtext(dc, "Press esc to hide this message...", normcol);
  mapdc(dc, win, mw, mh);
}

void drawshutdown(void) {
  const time_t now = time(NULL);
  struct tm *tm = localtime(&now);
  char string[100]; 
  strftime(string, 100, "It is now %I:%M %p, Shutting down computer.", tm);
  dc->x = 0;
  dc->y = 0;
  drawrect(dc, 0, 0, mw, mh, True, BG(dc, normcol));
  dc->x = mw/2-(dc->font.width*strlen(string))/2;
  dc->y = mh/2-(bh/2);
  drawtext(dc, string, normcol);
  mapdc(dc, win, mw, mh);
}

void
grabkeyboard(void) {
  int i;
  
  /* try to grab keyboard, we may have to wait for another process to ungrab */
  for(i = 0; i < 1000; i++) {
    if(XGrabKeyboard(dc->dpy, DefaultRootWindow(dc->dpy), True,
		     GrabModeAsync, GrabModeAsync, CurrentTime) == GrabSuccess)
      return;
    usleep(1000);
  }
  eprintf("cannot grab keyboard\n");
}

void releasekeyboard(void) {
  XUngrabKeyboard(dc->dpy, CurrentTime);
}

int
checkforescape(XKeyEvent *ev) {
  char buf[32];
  KeySym ksym = NoSymbol;
  Status status;
  
  XmbLookupString(xic, ev, buf, sizeof buf, &ksym, &status);
  if(status == XBufferOverflow)
    return 0;
  if(ksym==XK_Escape)
    return 1;
  return 0;
}

void
setup(void) {
	int x, y, screen = DefaultScreen(dc->dpy);
	Window root = RootWindow(dc->dpy, screen);
	XSetWindowAttributes swa;
	XIM xim;
#ifdef XINERAMA
	int n;
	XineramaScreenInfo *info;
#endif

	normcol[ColBG] = getcolor(dc, normbgcolor);
	normcol[ColFG] = getcolor(dc, normfgcolor);
	selcol[ColBG]  = getcolor(dc, selbgcolor);
	selcol[ColFG]  = getcolor(dc, selfgcolor);

	clip = XInternAtom(dc->dpy, "CLIPBOARD",   False);
	utf8 = XInternAtom(dc->dpy, "UTF8_STRING", False);

#ifdef XINERAMA
	if((info = XineramaQueryScreens(dc->dpy, &n))) {
		int a, j, di, i = 0, area = 0;
		unsigned int du;
		Window w, pw, dw, *dws;
		XWindowAttributes wa;

		XGetInputFocus(dc->dpy, &w, &di);
		if(w != root && w != PointerRoot && w != None) {
			/* find top-level window containing current input focus */
			do {
				if(XQueryTree(dc->dpy, (pw = w), &dw, &w, &dws, &du) && dws)
					XFree(dws);
			} while(w != root && w != pw);
			/* find xinerama screen with which the window intersects most */
			if(XGetWindowAttributes(dc->dpy, pw, &wa))
				for(j = 0; j < n; j++)
					if((a = INTERSECT(wa.x, wa.y, wa.width, wa.height, info[j])) > area) {
						area = a;
						i = j;
					}
		}
		/* no focused window is on screen, so use pointer location instead */
		if(!area && XQueryPointer(dc->dpy, root, &dw, &dw, &x, &y, &di, &di, &du))
			for(i = 0; i < n; i++)
				if(INTERSECT(x, y, 1, 1, info[i]))
					break;

		x = info[i].x_org;
		y = info[i].y_org;
		mh = info[i].height;
		mw = info[i].width;
		XFree(info);
	}
	else
#endif
	{
		x = 0;
		y = 0;
		mh = DisplayHeight(dc->dpy, screen);
		mw = DisplayWidth(dc->dpy, screen);
	}
	bh = dc->font.height + 2;

	/* create menu window */
	swa.override_redirect = True;
	swa.background_pixel = normcol[ColBG];
	swa.event_mask = ExposureMask | KeyPressMask | VisibilityChangeMask;
	win = XCreateWindow(dc->dpy, root, x, y, mw, mh, 0,
	                    DefaultDepth(dc->dpy, screen), CopyFromParent,
	                    DefaultVisual(dc->dpy, screen),
	                    CWOverrideRedirect | CWBackPixel | CWEventMask, &swa);

	/* open input methods */
	xim = XOpenIM(dc->dpy, NULL, NULL, NULL);
	xic = XCreateIC(xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
	                XNClientWindow, win, XNFocusWindow, win, NULL);

	XMapRaised(dc->dpy, win);
	resizedc(dc, mw, mh);
}
