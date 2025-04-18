//
// Created by jacob on 4/1/25.
//
#ifndef BAREWM_H
#define BAREWM_H
#include <X11/Xlib.h>

#define win        (client *t=0, *c=list; c && t!=list->prev; t=c, c=c->next)
#define ws_save(W) ws_list[W] = list;
#define ws_sel(W)  list = ws_list[ws = W]
#define MAX(a,b)   ((a) > (b) ? (a) : (b))
#define MIN(a,b)   ((a) < (b) ? (a) : (b))

#define win_size(W, gx, gy, gw, gh)                  \
    XGetGeometry(d, W, &(Window){0}, gx, gy, gw, gh, \
        &(unsigned int){0}, &(unsigned int){0})

// Taken from SOWM who took from DWM. Thank you both. https://github.com/dylanaraps/sowm/blob/master/sowm.h & https://git.suckless.org/dwm
#define mod_clean(mask) (mask & ~(numlock|LockMask) & \
    (ShiftMask|ControlMask|Mod1Mask|Mod2Mask|Mod3Mask|Mod4Mask|Mod5Mask))

typedef struct {
    const char** com;
    const int i;
    const Window w;
} Arg;

struct key {
    unsigned int mod;
    KeySym keysym;
    void (*function)(const Arg arg);
    const Arg arg;
};

typedef struct client {
    struct client *next, *prev;
    int f, wx, wy;
    unsigned int ww, wh;
    Window w;
} client;

void button_press(XEvent *e);
void button_release(XEvent *e);
void configure_request(XEvent *e);
void input_grab(Window root);
void key_press(XEvent *e);
void map_request(XEvent *e);
void mapping_notify(XEvent *e);
void notify_destroy(XEvent *e);
void notify_enter(XEvent *e);
void notify_motion(XEvent *e);
void run(const Arg arg);
void win_add(Window w);
void win_center(const Arg arg);
void win_del(Window w);
void win_fs(const Arg arg);
void win_focus(client *c);
void win_kill(const Arg arg);
void win_prev(const Arg arg);
void win_next(const Arg arg);
void win_to_ws(const Arg arg);
void ws_go(const Arg arg);
void win_tile(const Arg arg);
void tile(void);
void win_mw(const Arg arg);
void tile_mw(int mw);
void win_plan9(const Arg arg);
void draw_rectangle(int x1, int y1, int x2, int y2);
void launch_terminal_with_geometry(int x, int y, int width, int height);

static int xerror() { return 0; }

#endif // BAREWM_H
