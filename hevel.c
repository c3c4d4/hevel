#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
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

static const uint32_t border_color_active = 0xff285577;
static const uint32_t border_color_inactive = 0xff222222;
static const uint32_t border_width = 2;
static const uint32_t select_box_color = 0xffffffff;
static const uint32_t select_box_border = 2;
static const char *const select_havoc_app_id = "hevel-select";
static const int chord_click_timeout_ms = 125;

static struct {
	struct wl_display *display;
	struct wl_event_loop *evloop;
	struct wl_list windows;
	struct wl_list screens;
	struct swc_window *focused;
	struct {
		bool left, right;
		bool activated;
		bool selecting;
		struct wl_event_source *timer;
		int32_t start_x, start_y;
		int32_t cur_x, cur_y;
		struct wl_event_source *click_timer;
		struct {
			bool pending;
			bool forwarded;
			uint32_t button;
			uint32_t time;
		} click;
		struct {
			bool pending;
			struct swc_rectangle geometry;
		} spawn;
	} chord;
} hevel;

static void
focus_window(struct swc_window *swc, const char *reason)
{
	const char *from = hevel.focused && hevel.focused->title ? hevel.focused->title : "";
	const char *to = swc && swc->title ? swc->title : "";

	if(hevel.focused == swc)
		return;
	printf("focus %p ('%s') -> %p ('%s') (%s)\n",
	       (void *)hevel.focused, from, (void *)swc, to, reason);

	if(hevel.focused)
		swc_window_set_border(hevel.focused, border_color_inactive, border_width);

	swc_window_focus(swc);

	if(swc)
		swc_window_set_border(swc, border_color_active, border_width);

	hevel.focused = swc;
}

static bool
cursor_position(int32_t *x, int32_t *y)
{
	int32_t fx, fy;

	if(!swc_cursor_position(&fx, &fy))
		return false;
	*x = wl_fixed_to_int(fx);
	*y = wl_fixed_to_int(fy);
	return true;
}

static void
stop_select(void)
{
	if(hevel.chord.timer){
		wl_event_source_remove(hevel.chord.timer);
		hevel.chord.timer = NULL;
	}
	hevel.chord.selecting = false;
	swc_overlay_clear();
}

static int
select_tick(void *data)
{
	int32_t x, y;

	(void)data;
	if(!hevel.chord.selecting)
		return 0;

	if(cursor_position(&x, &y)){
		hevel.chord.cur_x = x;
		hevel.chord.cur_y = y;
		swc_overlay_set_box(hevel.chord.start_x, hevel.chord.start_y, x, y,
		                    select_box_color, select_box_border);
	}

	wl_event_source_timer_update(hevel.chord.timer, 16);
	return 0;
}

static void
spawn_havoc_select(const struct swc_rectangle *geometry)
{
	pid_t pid;

	hevel.chord.spawn.pending = true;
	hevel.chord.spawn.geometry = *geometry;

	pid = fork();
	if(pid == 0){
		execlp("havoc", "havoc", "-i", select_havoc_app_id, NULL);
		_exit(127);
	}
}

static int
click_timeout(void *data)
{
	(void)data;

	if(!hevel.chord.click.pending)
		return 0;

	if(hevel.chord.left && hevel.chord.right)
		return 0;

	if(!hevel.chord.click.forwarded){
		swc_pointer_send_button(hevel.chord.click.time, hevel.chord.click.button,
		                        WL_POINTER_BUTTON_STATE_PRESSED);
		hevel.chord.click.forwarded = true;
	}

	return 0;
}

static void
click_cancel(void)
{
	if(hevel.chord.click_timer){
		wl_event_source_remove(hevel.chord.click_timer);
		hevel.chord.click_timer = NULL;
	}
	hevel.chord.click.pending = false;
	hevel.chord.click.forwarded = false;
}

static void
windowdestroy(void *data)
{
	struct window *w = data;
	if(hevel.focused == w->swc)
		focus_window(NULL, "destroy");
	wl_list_remove(&w->link);
	free(w);
}

static void
windowentered(void *data)
{
	struct window *w = data;
	focus_window(w->swc, "hover");
}

static void
windowappidchanged(void *data)
{
	struct window *w = data;
	struct swc_rectangle geometry;
	bool is_select = hevel.chord.spawn.pending
	              && w->swc->app_id
	              && strcmp(w->swc->app_id, select_havoc_app_id) == 0;

	if(!is_select)
		return;

	geometry = hevel.chord.spawn.geometry;
	if(geometry.width < 50)
		geometry.width = 50;
	if(geometry.height < 50)
		geometry.height = 50;
	swc_window_set_geometry(w->swc, &geometry);
	hevel.chord.spawn.pending = false;
}

static const struct swc_window_handler windowhandler = {
	.destroy = windowdestroy,
	.app_id_changed = windowappidchanged,
	.entered = windowentered,
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
	struct swc_rectangle geometry;
	bool is_select = hevel.chord.spawn.pending
	              && swc->app_id
	              && strcmp(swc->app_id, select_havoc_app_id) == 0;

	w = malloc(sizeof(*w));
	if(!w)
		return;
	w->swc = swc;
	wl_list_insert(&hevel.windows, &w->link);
	swc_window_set_handler(swc, &windowhandler, w);
	swc_window_set_stacked(swc);
	swc_window_set_border(swc, border_color_inactive, border_width);
	if(is_select){
		geometry = hevel.chord.spawn.geometry;
		if(geometry.width < 50)
			geometry.width = 50;
		if(geometry.height < 50)
			geometry.height = 50;
		swc_window_set_geometry(swc, &geometry);
		hevel.chord.spawn.pending = false;
	}
	swc_window_show(swc);
	printf("window '%s'\n", swc->title ? swc->title : "");
	focus_window(swc, "new_window");
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
	bool pressed;
	int32_t x, y;
	struct swc_rectangle geometry;
	bool was_left = hevel.chord.left;
	bool was_right = hevel.chord.right;
	bool handle_chord;

	(void)data;
	(void)time;

	pressed = (state == WL_POINTER_BUTTON_STATE_PRESSED);

	switch(b){
	case BTN_LEFT:
		name = "left";
		hevel.chord.left = pressed;
		break;
	case BTN_MIDDLE:
		name = "middle";
		break;
	case BTN_RIGHT:
		name = "right";
		hevel.chord.right = pressed;
		break;
	default:
		name = "unknown";
		break;
	}

	printf("button %s (%d) %s\n", name, b, pressed ? "pressed" : "released");

	handle_chord = (b == BTN_LEFT || b == BTN_RIGHT);

	if(hevel.chord.left && hevel.chord.right && !hevel.chord.activated){
		click_cancel();
		hevel.chord.activated = true;
		if(cursor_position(&x, &y)){
			hevel.chord.selecting = true;
			hevel.chord.start_x = x;
			hevel.chord.start_y = y;
			hevel.chord.cur_x = x;
			hevel.chord.cur_y = y;
			swc_overlay_set_box(x, y, x, y, select_box_color, select_box_border);
			if(!hevel.chord.timer)
				hevel.chord.timer = wl_event_loop_add_timer(hevel.evloop, select_tick, NULL);
			if(hevel.chord.timer)
				wl_event_source_timer_update(hevel.chord.timer, 16);
		}
	}

	/* while a chord is active swallow left/right events so they don't go to clients */
	if(handle_chord && hevel.chord.activated && !hevel.chord.selecting){
		if(!hevel.chord.left && !hevel.chord.right)
			hevel.chord.activated = false;
		return;
	}

	/* pass normal clicks through to clients */
	if(handle_chord && pressed && !hevel.chord.selecting){
		bool other_down = (b == BTN_LEFT) ? was_right : was_left;
		if(other_down){
			/* chord will activate via the block above */
		} else if(!hevel.chord.click.pending) {
			hevel.chord.click.pending = true;
			hevel.chord.click.forwarded = false;
			hevel.chord.click.button = b;
			hevel.chord.click.time = time;
			if(!hevel.chord.click_timer)
				hevel.chord.click_timer = wl_event_loop_add_timer(hevel.evloop, click_timeout, NULL);
			if(hevel.chord.click_timer)
				wl_event_source_timer_update(hevel.chord.click_timer, chord_click_timeout_ms);
			return;
		}
	}

	if(handle_chord && !pressed && !hevel.chord.selecting){
		if(hevel.chord.click.pending && hevel.chord.click.button == b){
			if(!hevel.chord.click.forwarded){
				swc_pointer_send_button(hevel.chord.click.time, hevel.chord.click.button,
				                        WL_POINTER_BUTTON_STATE_PRESSED);
			}
			swc_pointer_send_button(time, b, WL_POINTER_BUTTON_STATE_RELEASED);
			click_cancel();
			return;
		}
		swc_pointer_send_button(time, b, WL_POINTER_BUTTON_STATE_RELEASED);
		return;
	}

	if(b == BTN_RIGHT && !pressed && hevel.chord.selecting){
		int32_t x1, y1, x2, y2;
		uint32_t outer_w, outer_h;
		uint32_t bw = border_width;

		if(!cursor_position(&x, &y)){
			x = hevel.chord.cur_x;
			y = hevel.chord.cur_y;
		}
		stop_select();

		x1 = hevel.chord.start_x < x ? hevel.chord.start_x : x;
		y1 = hevel.chord.start_y < y ? hevel.chord.start_y : y;
		x2 = hevel.chord.start_x < x ? x : hevel.chord.start_x;
		y2 = hevel.chord.start_y < y ? y : hevel.chord.start_y;
		outer_w = (uint32_t)abs(x2 - x1);
		outer_h = (uint32_t)abs(y2 - y1);
		if(outer_w < (50 + 2 * bw))
			outer_w = 50 + 2 * bw;
		if(outer_h < (50 + 2 * bw))
			outer_h = 50 + 2 * bw;

		/* swc_window_set_*  content geom */
		geometry.x = x1 + (int32_t)bw;
		geometry.y = y1 + (int32_t)bw;
		geometry.width = outer_w > 2 * bw ? outer_w - 2 * bw : 1;
		geometry.height = outer_h > 2 * bw ? outer_h - 2 * bw : 1;
		spawn_havoc_select(&geometry);
		printf("spawned havoc at %d,%d %ux%u\n", geometry.x, geometry.y, geometry.width, geometry.height);
	}

	if(!handle_chord){
		swc_pointer_send_button(time, b, state);
		return;
	}

	if(!hevel.chord.left && !hevel.chord.right)
		hevel.chord.activated = false;
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
	hevel.evloop = evloop;
	if(!swc_initialize(hevel.display, evloop, &manager)){
		fprintf(stderr, "cannot initialize swc\n");
		return 1;
	}

	swc_add_binding(SWC_BINDING_KEY, SWC_MOD_LOGO | SWC_MOD_SHIFT,
	                XKB_KEY_q, quit, NULL);
	swc_add_binding(SWC_BINDING_BUTTON, SWC_MOD_ANY, BTN_LEFT, button, NULL);
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
