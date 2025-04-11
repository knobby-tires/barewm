// barewm - An entirely new and totally original computing experience, I promise.

#include <X11/Xlib.h>
#include <X11/XF86keysym.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>

#include "barewm.h"

// For debug logging
FILE *debug_file = NULL;

static client       *list = {0}, *ws_list[10] = {0}, *cur;
static int          ws = 1, sw, sh, wx, wy, numlock = 0;
static unsigned int ww, wh;
static int tm = 0;
static int master_width = -1;
static int plan9_mode = 0;

void draw_rectangle(int x1, int y1, int x2, int y2);
void launch_terminal_with_geometry(int x, int y, int width, int height);

static Display      *d;
static XButtonEvent mouse;
static Window       root;

static void (*events[LASTEvent])(XEvent *e) = {
    [ButtonPress]      = button_press,
    [ButtonRelease]    = button_release,
    [ConfigureRequest] = configure_request,
    [KeyPress]         = key_press,
    [MapRequest]       = map_request,
    [MappingNotify]    = mapping_notify,
    [DestroyNotify]    = notify_destroy,
    [EnterNotify]      = notify_enter,
    [MotionNotify]     = notify_motion
};

#include "config.h"

// Simple debug logging function
void debug_log(const char *message) {
    if (!debug_file) {
        debug_file = fopen("debug.log", "w");
        if (!debug_file) return;
    }

    fprintf(debug_file, "%s\n", message);
    fflush(debug_file);
}

void win_focus(client *c) {
    cur = c;
    XSetInputFocus(d, cur->w, RevertToParent, CurrentTime);
}

void notify_destroy(XEvent *e) {
    win_del(e->xdestroywindow.window);

    if (list) win_focus(list->prev);
}

void notify_enter(XEvent *e) {
    while(XCheckTypedEvent(d, EnterNotify, e));

    for win if (c->w == e->xcrossing.window) win_focus(c);
}

void notify_motion(XEvent *e) {
    static int prev_x = 0, prev_y = 0;

    if (plan9_mode && mouse.button) {
        // Erase previous rectangle if it exists
        if (prev_x || prev_y) {
            draw_rectangle(wx, wy, prev_x, prev_y);
        }

        // Draw new rectangle
        draw_rectangle(wx, wy, e->xbutton.x_root, e->xbutton.y_root);

        // Remember current position for next motion
        prev_x = e->xbutton.x_root;
        prev_y = e->xbutton.y_root;
        return;
    } else {
        // Reset previous position when not in Plan9 mode
        prev_x = prev_y = 0;
    }

    if (!mouse.subwindow || cur->f || tm) return;

    while(XCheckTypedEvent(d, MotionNotify, e));

    int xd = e->xbutton.x_root - mouse.x_root;
    int yd = e->xbutton.y_root - mouse.y_root;

    XMoveResizeWindow(d, mouse.subwindow,
        wx + (mouse.button == 1 ? xd : 0),
        wy + (mouse.button == 1 ? yd : 0),
        MAX(1, ww + (mouse.button == 3 ? xd : 0)),
        MAX(1, wh + (mouse.button == 3 ? yd : 0)));
}

void key_press(XEvent *e) {
    KeySym keysym = XkbKeycodeToKeysym(d, e->xkey.keycode, 0, 0);
    unsigned int state = e->xkey.state;

    debug_log("Key press detected");

    for (unsigned int i=0; i < sizeof(keys)/sizeof(*keys); ++i) {
        if (keys[i].keysym == keysym &&
            mod_clean(keys[i].mod) == mod_clean(state)) {
            debug_log("Key binding matched, calling function");
            keys[i].function(keys[i].arg);
        }
    }
}

void button_press(XEvent *e) {
    debug_log("Button press event");

    if (plan9_mode) {
        debug_log("Plan9 mode: Button press event");
        mouse = e->xbutton;
        wx = e->xbutton.x_root;
        wy = e->xbutton.y_root;
        return;  // Important: return here
    }

    if (!e->xbutton.subwindow) return;

    win_size(e->xbutton.subwindow, &wx, &wy, &ww, &wh);
    XRaiseWindow(d, e->xbutton.subwindow);
    mouse = e->xbutton;
}

void button_release(XEvent *e) {
    if (plan9_mode) {
        // Erase the last rectangle
        draw_rectangle(wx, wy, e->xbutton.x_root, e->xbutton.y_root);

        // Calculate rectangle dimensions
        int x = MIN(wx, e->xbutton.x_root);
        int y = MIN(wy, e->xbutton.y_root);
        int width = abs(e->xbutton.x_root - wx);
        int height = abs(e->xbutton.y_root - wy);

        // Only launch if rectangle is reasonably sized
        if (width > 30 && height > 30) {
            // Reset Plan9 mode before launching terminal
            plan9_mode = 0;
            XDefineCursor(d, root, XCreateFontCursor(d, 68)); // normal cursor

            // For st terminal: calculate character-based dimensions
            int char_width = 8;  // Adjust based on your font
            int char_height = 16; // Adjust based on your font

            // Calculate dimensions in characters
            int cols = width / char_width;
            int rows = height / char_height;

            // Minimum terminal size
            if (cols < 10) cols = 10;
            if (rows < 5) rows = 5;

            // Create geometry string
            char geom[64];
            sprintf(geom, "%dx%d+%d+%d", cols, rows, x, y);

            // Use fork+exec to launch terminal with geometry
            pid_t pid = fork();
            if (pid == 0) {
                // Child process
                if (d) close(ConnectionNumber(d));

                // Execute terminal with geometry
                setsid();
                char *args[] = {"st", "-g", geom, NULL};
                execvp(args[0], args);
                exit(1);
            }
        } else {
            // Reset Plan9 mode even if no terminal is launched
            plan9_mode = 0;
            XDefineCursor(d, root, XCreateFontCursor(d, 68)); // normal cursor
        }

        return;
    }

    // Original code
    mouse.subwindow = 0;
}

void launch_terminal_with_geometry(int x, int y, int width, int height) {
    char geom[32];
    sprintf(geom, "%dx%d+%d+%d", width/8, height/16, x, y); // Approximate character size conversion

    if (fork()) return;
    if (d) close(ConnectionNumber(d));

    setsid();

    // Execute st with the geometry parameter
    char *args[] = {"st", "-g", geom, NULL};
    execvp(args[0], args);
    exit(0);
}

void win_add(Window w) {
    client *c;

    if (!(c = (client *) calloc(1, sizeof(client))))
        exit(1);

    c->w = w;

    if (list) {
        list->prev->next = c;
        c->prev          = list->prev;
        list->prev       = c;
        c->next          = list;
    } else {
        list = c;
        list->prev = list->next = list;
    }

    ws_save(ws);
    if (tm) tile();
}

void win_del(Window w) {
    client *x = 0;

    for win if (c->w == w) x = c;

    if (!list || !x)  return;
    if (x->prev == x) list = 0;
    if (list == x)    list = x->next;
    if (x->next)      x->next->prev = x->prev;
    if (x->prev)      x->prev->next = x->next;

    free(x);
    ws_save(ws);
    if (tm) tile();
}

void win_kill(const Arg arg) {
    if (cur) XKillClient(d, cur->w);
}

void win_center(const Arg arg) {
    if (!cur) return;

    win_size(cur->w, &(int){0}, &(int){0}, &ww, &wh);
    XMoveWindow(d, cur->w, (sw - ww) / 2, (sh - wh) / 2);
}

void win_fs(const Arg arg) {
    if (!cur) return;

    if ((cur->f = cur->f ? 0 : 1)) {
        win_size(cur->w, &cur->wx, &cur->wy, &cur->ww, &cur->wh);
        XMoveResizeWindow(d, cur->w, 0, 0, sw, sh);
    } else {
        XMoveResizeWindow(d, cur->w, cur->wx, cur->wy, cur->ww, cur->wh);
    }
}

void win_to_ws(const Arg arg) {
    int tmp = ws;

    if (arg.i == tmp) return;

    ws_sel(arg.i);
    win_add(cur->w);
    ws_save(arg.i);

    ws_sel(tmp);
    win_del(cur->w);
    XUnmapWindow(d, cur->w);
    ws_save(tmp);

    if (list) win_focus(list);
}

void win_prev(const Arg arg) {
    if (!cur) return;

    XRaiseWindow(d, cur->prev->w);
    win_focus(cur->prev);
}

void win_next(const Arg arg) {
    if (!cur) return;

    XRaiseWindow(d, cur->next->w);
    win_focus(cur->next);
}

void win_tile(const Arg arg) {
    tm = !tm;

    if (tm) tile();
    else {
        for win if (c) XMoveResizeWindow(d, c->w, c->wx, c->wy, c->ww, c->wh);
    }
}

void win_plan9(const Arg arg) {
    // Toggle Plan9 mode
    plan9_mode = !plan9_mode;

    if (plan9_mode) {
        // Change cursor to indicate we're in Plan9 mode
        XDefineCursor(d, root, XCreateFontCursor(d, 52)); // crosshair cursor

        // Ensure we grab pointer for correct drawing
        XGrabPointer(d, root, False,
                  ButtonPressMask|ButtonReleaseMask|PointerMotionMask,
                  GrabModeAsync, GrabModeAsync, None, None, CurrentTime);
    } else {
        // Restore normal cursor and ungrab pointer
        XDefineCursor(d, root, XCreateFontCursor(d, 68)); // normal cursor
        XUngrabPointer(d, CurrentTime);
    }
}

void draw_rectangle(int x1, int y1, int x2, int y2) {
    static GC gc = 0;

    if (!gc) {
        XGCValues gcv;
        gcv.function = GXxor;            // XOR drawing mode
        gcv.foreground = 0xffffff;       // White (shows well on most backgrounds)
        gcv.line_width = 2;              // Thicker line for visibility
        gcv.subwindow_mode = IncludeInferiors;  // Draw over subwindows too

        gc = XCreateGC(d, root,
                    GCFunction|GCForeground|GCLineWidth|GCSubwindowMode,
                    &gcv);
    }

    // Calculate rectangle dimensions (handle any direction of dragging)
    int x = MIN(x1, x2);
    int y = MIN(y1, y2);
    int width = abs(x2 - x1);
    int height = abs(y2 - y1);

    // XOR mode - drawing twice erases the rectangle
    XDrawRectangle(d, root, gc, x, y, width, height);
    XFlush(d);
}

void tile_mw(int mw) {
    master_width = mw;

    int n = 0;
    for win n++;

    if (n == 1) {
        XMoveResizeWindow(d, list->w, 0, 0, sw, sh);
        return;
    }

    int cw = sw - mw;
    int ch = sh / (n - 1);

    client *c = list;
    win_size(c->w, &c->wx, &c->wy, &c->ww, &c->wh);
    XMoveResizeWindow(d, c->w, 0, 0, mw, sh);

    c = c->next;
    int i = 0;
    while (c != list) {
        win_size(c->w, &c->wx, &c->wy, &c->ww, &c->wh);
        XMoveResizeWindow(d, c->w, mw, i * ch, cw, ch);
        c = c->next;
        i++;
    }
}

void tile(void) {
    if (!list) return;

    int mw = (master_width == -1) ? sw * 0.5 : master_width;
    tile_mw(mw);
}

void win_mw(const Arg arg) {
    int new_mw = (master_width == -1) ? (sw * 0.5) : master_width;

    new_mw += arg.i;

    if (new_mw < 100) new_mw = 100;
    if (new_mw > sw - 100) new_mw = sw - 100;

    tile_mw(new_mw);
    XSync(d, False);
}

void ws_go(const Arg arg) {
    int tmp = ws;

    if (arg.i == ws) return;

    ws_save(ws);
    ws_sel(arg.i);

    for win XMapWindow(d, c->w);

    ws_sel(tmp);

    for win XUnmapWindow(d, c->w);

    ws_sel(arg.i);

    if (list) win_focus(list); else cur = 0;
    if (tm) tile();
}

void configure_request(XEvent *e) {
    XConfigureRequestEvent *ev = &e->xconfigurerequest;

    XConfigureWindow(d, ev->window, ev->value_mask, &(XWindowChanges) {
        .x          = ev->x,
        .y          = ev->y,
        .width      = ev->width,
        .height     = ev->height,
        .sibling    = ev->above,
        .stack_mode = ev->detail
    });
}

void map_request(XEvent *e) {
    Window w = e->xmaprequest.window;

    XSelectInput(d, w, StructureNotifyMask|EnterWindowMask);
    win_size(w, &wx, &wy, &ww, &wh);
    win_add(w);
    cur = list->prev;

    if (tm) { tile(); }
    else { if (wx + wy == 0) win_center((Arg){0});}

    XMapWindow(d, w);
    win_focus(list->prev);
}

void mapping_notify(XEvent *e) {
    XMappingEvent *ev = &e->xmapping;

    if (ev->request == MappingKeyboard || ev->request == MappingModifier) {
        XRefreshKeyboardMapping(ev);
        input_grab(root);
    }
}

void run(const Arg arg) {
    if (fork()) return;
    if (d) close(ConnectionNumber(d));

    setsid();
    execvp((char*)arg.com[0], (char**)arg.com);
}

void input_grab(Window root) {
    unsigned int i, j, modifiers[] = {0, LockMask, numlock, numlock|LockMask};
    XModifierKeymap *modmap = XGetModifierMapping(d);
    KeyCode code;

    for(i = 0; i < 8; i++){
        for (int k = 0; k < modmap->max_keypermod; k++)
            if ((modmap->modifiermap[i * modmap->max_keypermod + k]
                == XKeysymToKeycode(d, 0xff7f)))
                numlock = (1 << i);
    }

    XUngrabKey(d, AnyKey, AnyModifier, root);

    for (i = 0; i < sizeof(keys)/sizeof(*keys); i++)
        if ((code = XKeysymToKeycode(d, keys[i].keysym)))
            for (j = 0; j < sizeof(modifiers)/sizeof(*modifiers); j++)
                XGrabKey(d, code, keys[i].mod | modifiers[j], root,
                        True, GrabModeAsync, GrabModeAsync);

    for (i = 1; i < 4; i += 2)
        for (j = 0; j < sizeof(modifiers)/sizeof(*modifiers); j++)
            XGrabButton(d, i, MOD | modifiers[j], root, True,
                ButtonPressMask|ButtonReleaseMask|PointerMotionMask,
                GrabModeAsync, GrabModeAsync, 0, 0);

    XFreeModifiermap(modmap);
}

int main(void) {
    XEvent ev;

    debug_file = fopen("debug.log", "w");
    debug_log("barewm starting up");

    if (!(d = XOpenDisplay(0))) exit(1);

    XSync(d, False);

    signal(SIGCHLD, SIG_IGN);
    XSetErrorHandler(xerror);

    int s = DefaultScreen(d);
    root  = RootWindow(d, s);
    sw    = XDisplayWidth(d, s);
    sh    = XDisplayHeight(d, s);

    XSelectInput(d,  root, SubstructureRedirectMask);
    XDefineCursor(d, root, XCreateFontCursor(d, 68));
    input_grab(root);

    debug_log("barewm initialized, entering main loop");

    while (1 && !XNextEvent(d, &ev)) {
        char buf[32];
        sprintf(buf, "Event received: type=%d", ev.type);
        debug_log(buf);
        if (events[ev.type]) events[ev.type](&ev);
    }

    if (debug_file) fclose(debug_file);
    return 0;
}
