#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <wayland-server.h>
#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <swc.h>

struct window {
	struct swc_window *swc;
	struct wl_list link;
};

struct screen {
	struct swc_screen *swc;
	struct wl_list link;
};

static struct {
	struct wl_display *display;
	struct wl_list windows;
	struct wl_list screens;
} hevel;

static void
windowdestroy(void *data)
{
	struct window *w = data;
	wl_list_remove(&w->link);
	free(w);
}

static const struct swc_window_handler windowhandler = {
	.destroy = windowdestroy,
};

static void
screendestroy(void *data)
{
	struct screen *s = data;
	wl_list_remove(&s->link);
	free(s);
}

static const struct swc_screen_handler screenhandler = {
	.destroy = screendestroy,
};

static void
newscreen(struct swc_screen *swc)
{
	struct screen *s;

	s = malloc(sizeof(*s));
	if(!s)
		return;
	s->swc = swc;
	wl_list_insert(&hevel.screens, &s->link);
	swc_screen_set_handler(swc, &screenhandler, s);
	printf("screen %dx%d\n", swc->geometry.width, swc->geometry.height);
}

static void
newwindow(struct swc_window *swc)
{
	struct window *w;

	w = malloc(sizeof(*w));
	if(!w)
		return;
	w->swc = swc;
	wl_list_insert(&hevel.windows, &w->link);
	swc_window_set_handler(swc, &windowhandler, w);
	swc_window_set_stacked(swc);
	swc_window_show(swc);
	printf("window '%s'\n", swc->title ? swc->title : "");
}

static void
newdevice(struct libinput_device *dev)
{
	(void)dev;
}

static const struct swc_manager manager = {
	.new_screen = newscreen,
	.new_window = newwindow,
	.new_device = newdevice,
};

static void
button(void *data, uint32_t time, uint32_t b, uint32_t state)
{
	const char *name;

	(void)data;
	(void)time;

	switch(b){
	case BTN_LEFT:   name = "left"; break;
	case BTN_MIDDLE: name = "middle"; break;
	case BTN_RIGHT:  name = "right"; break;
	default:         name = "unknown"; break;
	}

	printf("button %s (%d) %s\n", name, b,
	       state == WL_POINTER_BUTTON_STATE_PRESSED ? "pressed" : "released");
}

static void
quit(void *data, uint32_t time, uint32_t value, uint32_t state)
{
	(void)data;
	(void)time;
	(void)value;
	(void)state;
	wl_display_terminate(hevel.display);
}

static void
sig(int s)
{
	(void)s;
	wl_display_terminate(hevel.display);
}

int
main(void)
{
	struct wl_event_loop *evloop;
	const char *sock;

	wl_list_init(&hevel.windows);
	wl_list_init(&hevel.screens);

	hevel.display = wl_display_create();
	if(!hevel.display){
		fprintf(stderr, "cannot create display\n");
		return 1;
	}

	evloop = wl_display_get_event_loop(hevel.display);
	if(!swc_initialize(hevel.display, evloop, &manager)){
		fprintf(stderr, "cannot initialize swc\n");
		return 1;
	}

	swc_add_binding(SWC_BINDING_KEY, SWC_MOD_LOGO | SWC_MOD_SHIFT,
	                XKB_KEY_q, quit, NULL);
	swc_add_binding(SWC_BINDING_BUTTON, SWC_MOD_ANY, BTN_LEFT, button, NULL);
	swc_add_binding(SWC_BINDING_BUTTON, SWC_MOD_ANY, BTN_MIDDLE, button, NULL);
	swc_add_binding(SWC_BINDING_BUTTON, SWC_MOD_ANY, BTN_RIGHT, button, NULL);

	sock = wl_display_add_socket_auto(hevel.display);
	if(!sock){
		fprintf(stderr, "cannot add socket\n");
		return 1;
	}

	printf("%s\n", sock);
	setenv("WAYLAND_DISPLAY", sock, 1);

	signal(SIGTERM, sig);
	signal(SIGINT, sig);

	wl_display_run(hevel.display);

	swc_finalize();
	wl_display_destroy(hevel.display);

	return 0;
}
