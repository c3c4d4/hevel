#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <wayland-server.h>
#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <swc.h>

#include "config.h"
#include "nein_cursor.h"

#include "protocol/hevel-server-protocol.h"

struct window {
	struct swc_window *swc;
	struct wl_list link;
};

struct screen {
	struct swc_screen *swc;
	struct wl_list link;
};

static const int scrollpx = 64;
static const int scrollms = 16;
static const int scrollease = 4;
static const int scrollcap = 64;

static int scrollpos = 0;
static struct wl_list scrollpos_resources;

static bool debugscroll = false;
static struct {
	struct wl_display *display;
	struct wl_event_loop *evloop;
	struct wl_list windows;
	struct wl_list screens;
	struct swc_window *focused;
	struct {
		bool left, middle, right;
		bool activated;
		bool killing;
		bool scrolling;
		bool auto_scrolling;
		bool moving;
		bool resize;
		int32_t move_start_win_x, move_start_win_y;
		int32_t move_start_cursor_x, move_start_cursor_y;
		int32_t scroll_rem;
		int32_t scroll_pending_px;
		int8_t scroll_cursor_dir;
		struct wl_event_source *scroll_timer;
		struct swc_window *scroll_last;
		struct swc_rectangle scroll_last_geo;
		int32_t scroll_last_step;
		bool selecting;
		struct wl_event_source *timer;
		int32_t start_x, start_y;
		int32_t cur_x, cur_y;
		struct wl_event_source *click_timer;
		struct wl_event_source *move_scroll_timer;
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

static int scroll_tick(void *data);
static void scroll_stop(void);

static void
remove_resource(struct wl_resource *resource)
{
	wl_list_remove(wl_resource_get_link(resource));
}

void 
bind_scrollpos(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
	(void)data;
	struct wl_resource *resource;

	if (version >= 1)
		version = 1;

	resource = wl_resource_create(client, &hevel_scroll_interface, version, id);

	if(!resource) {
		wl_client_post_no_memory(client);
		return;
	}

	wl_resource_set_implementation(resource, NULL, NULL, remove_resource);
	wl_list_insert(&scrollpos_resources, wl_resource_get_link(resource));

	hevel_scroll_send_get_pos(resource, scrollpos);
}

void
send_scrollpos(void)
{
	struct wl_resource *resource;
	
	wl_resource_for_each(resource, &scrollpos_resources)
		hevel_scroll_send_get_pos(resource, scrollpos);
}

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
		swc_window_set_border(hevel.focused, inner_border_color_inactive, inner_border_width, outer_border_color_inactive, outer_border_width);

	swc_window_focus(swc);

	if(swc)
		swc_window_set_border(swc, inner_border_color_active, inner_border_width, outer_border_color_active, outer_border_width);

	hevel.focused = swc;

	/* center the focused window */
	if (swc && !wl_list_empty(&hevel.screens)) {
		struct swc_rectangle window_geom;
		struct screen *screen = wl_container_of(hevel.screens.next, screen, link);

		if (swc_window_get_geometry(swc, &window_geom)) {
			int32_t window_center_y = window_geom.y + (int32_t)window_geom.height / 2;
			int32_t screen_center_y = screen->swc->geometry.y + (int32_t)screen->swc->geometry.height / 2;
			int32_t scroll_delta = screen_center_y - window_center_y;

			if (scroll_delta != 0) {
				/* stop scroll before auto-scroll */
				scroll_stop();

				hevel.chord.scroll_pending_px = scroll_delta;
				hevel.chord.scroll_rem = 0;
				hevel.chord.auto_scrolling = true;

				if (!hevel.chord.scroll_timer) {
					hevel.chord.scroll_timer = wl_event_loop_add_timer(
						hevel.evloop, scroll_tick, NULL);
				}
				wl_event_source_timer_update(hevel.chord.scroll_timer, scrollms);
			}
		}
	}
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
update_mode_cursor(void)
{
	if (hevel.chord.killing)
		swc_set_cursor(SWC_CURSOR_SIGHT);
	else if (hevel.chord.scrolling) {
		if (hevel.chord.scroll_cursor_dir < 0)
			swc_set_cursor(SWC_CURSOR_UP);
		else
			swc_set_cursor(SWC_CURSOR_DOWN);
	}
	else if (hevel.chord.selecting)
		swc_set_cursor(SWC_CURSOR_CROSS);
	else if (hevel.chord.moving || hevel.chord.resize)
		swc_set_cursor(SWC_CURSOR_BOX);
	else
		swc_set_cursor(SWC_CURSOR_DEFAULT);
}

static void
maybe_enable_nein_cursor_theme(void)
{
	const struct nein_cursor_meta *arrow = &nein_cursor_metadata[NEIN_CURSOR_WHITEARROW];
	const struct nein_cursor_meta *box = &nein_cursor_metadata[NEIN_CURSOR_BOXCURSOR];
	const struct nein_cursor_meta *cross = &nein_cursor_metadata[NEIN_CURSOR_CROSSCURSOR];
	const struct nein_cursor_meta *sight = &nein_cursor_metadata[NEIN_CURSOR_SIGHTCURSOR];
	const struct nein_cursor_meta *up = &nein_cursor_metadata[NEIN_CURSOR_T];
	const struct nein_cursor_meta *down = &nein_cursor_metadata[NEIN_CURSOR_B];

	if (!cursor_theme || strcmp(cursor_theme, "nein") != 0)
		return;

	swc_set_cursor_mode(SWC_CURSOR_MODE_COMPOSITOR);
	swc_set_cursor_image(SWC_CURSOR_DEFAULT, &nein_cursor_data[arrow->offset],
	                     arrow->width, arrow->height,
	                     arrow->hotspot_x, arrow->hotspot_y);
	swc_set_cursor_image(SWC_CURSOR_BOX, &nein_cursor_data[box->offset],
	                     box->width, box->height,
	                     box->hotspot_x, box->hotspot_y);
	swc_set_cursor_image(SWC_CURSOR_CROSS, &nein_cursor_data[cross->offset],
	                     cross->width, cross->height,
	                     cross->hotspot_x, cross->hotspot_y);
	swc_set_cursor_image(SWC_CURSOR_SIGHT, &nein_cursor_data[sight->offset],
	                     sight->width, sight->height,
	                     sight->hotspot_x, sight->hotspot_y);
	swc_set_cursor_image(SWC_CURSOR_UP, &nein_cursor_data[up->offset],
	                     up->width, up->height,
	                     up->hotspot_x, up->hotspot_y);
	swc_set_cursor_image(SWC_CURSOR_DOWN, &nein_cursor_data[down->offset],
	                     down->width, down->height,
	                     down->hotspot_x, down->hotspot_y);

	update_mode_cursor();
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
	update_mode_cursor();
}

static int scroll_tick(void *data);

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

static int
move_scroll_tick(void *data)
{
	int32_t x, y;
	struct screen *s;
	struct swc_rectangle geometry;
	int32_t screen_height = 0;

	(void)data;
	if(!hevel.chord.moving)
		return 0;

	/* get screen size,  only first screen */
	wl_list_for_each(s, &hevel.screens, link){
		screen_height = s->swc->geometry.height;
		break;
	}

	if(screen_height == 0){
		wl_event_source_timer_update(hevel.chord.move_scroll_timer, 16);
		return 0;
	}

	if(!cursor_position(&x, &y)){
		wl_event_source_timer_update(hevel.chord.move_scroll_timer, 16);
		return 0;
	}

	/* get where the where the window starts [line 558], every 16ms calculate where it should be
	 * then move only <config-value>% of that gap, then next frame, move
	 * <config-value>% of the new, smaller gap, exponential easing*/
	if(hevel.focused && swc_window_get_geometry(hevel.focused, &geometry)){
		int32_t target_x = hevel.chord.move_start_win_x + (x - hevel.chord.move_start_cursor_x);
		int32_t target_y = hevel.chord.move_start_win_y + (y - hevel.chord.move_start_cursor_y);
		int32_t new_x = geometry.x + (int32_t)((target_x - geometry.x) * move_ease_factor);
		int32_t new_y = geometry.y + (int32_t)((target_y - geometry.y) * move_ease_factor);
		swc_window_set_position(hevel.focused, new_x, new_y);
	}

	/* check near top bottom and scroll accordingly */
	if(y < move_scroll_edge_threshold){
		hevel.chord.scroll_pending_px += move_scroll_speed;
		if(!hevel.chord.scroll_timer)
			hevel.chord.scroll_timer = wl_event_loop_add_timer(hevel.evloop, scroll_tick, NULL);
		if(hevel.chord.scroll_timer)
			wl_event_source_timer_update(hevel.chord.scroll_timer, 1);
	} else if(y > screen_height - move_scroll_edge_threshold){
		hevel.chord.scroll_pending_px -= move_scroll_speed;
		if(!hevel.chord.scroll_timer)
			hevel.chord.scroll_timer = wl_event_loop_add_timer(hevel.evloop, scroll_tick, NULL);
		if(hevel.chord.scroll_timer)
			wl_event_source_timer_update(hevel.chord.scroll_timer, 1);
	}

	wl_event_source_timer_update(hevel.chord.move_scroll_timer, 16);
	return 0;
}

static void
spawn_term_select(const struct swc_rectangle *geometry)
{
	pid_t pid;

	hevel.chord.spawn.pending = true;
	hevel.chord.spawn.geometry = *geometry;

	pid = fork();
	if(pid == 0){
		execlp(term, term, "-i", select_term_app_id, NULL);
		_exit(127);
	}
}

static void click_cancel(void);

static int
click_timeout(void *data)
{
	(void)data;

	if(!hevel.chord.click.pending)
		return 0;

	/* don't forward clicks while move chord is active */
	if (hevel.chord.moving) {
		click_cancel();
		return 0;
	}

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
scroll_stop(void)
{
	hevel.chord.scroll_pending_px = 0;
	hevel.chord.scroll_rem = 0;
	hevel.chord.scroll_last = NULL;
	hevel.chord.scroll_last_step = 0;
	hevel.chord.auto_scrolling = false;
}

static int
scroll_tick(void *data)
{
	struct window *w, *tmp;
	struct swc_rectangle geometry;
	int32_t rem = hevel.chord.scroll_pending_px;
	int32_t step;
	static unsigned tickno;

	(void)data;

	if (!hevel.chord.scroll_timer) {
		if (debugscroll)
			fprintf(stderr, "[scroll] tick with no timer\n");
		return 0;
	}

	if ((!hevel.chord.scrolling && !hevel.chord.auto_scrolling && !hevel.chord.moving) || rem == 0) {
		if (debugscroll && tickno % 10 == 0)
			fprintf(stderr, "[scroll] tick stop scrolling=%d auto_scrolling=%d moving=%d rem=%d\n", hevel.chord.scrolling, hevel.chord.auto_scrolling, hevel.chord.moving, rem);
		scroll_stop();
		return 0;
	}

	step = rem / scrollease;
	if (step == 0)
		step = rem > 0 ? 1 : -1;
	if (step > scrollcap)
		step = scrollcap;
	if (step < -scrollcap)
		step = -scrollcap;

	if (debugscroll && (++tickno % 10 == 0 || step == scrollcap || step == -scrollcap)) {
		fprintf(stderr, "[scroll] tick rem=%d step=%d pending=%d last=%p\n",
		        rem, step, hevel.chord.scroll_pending_px, (void *)hevel.chord.scroll_last);
	}

	scrollpos += step;
	send_scrollpos();

	wl_list_for_each_safe(w, tmp, &hevel.windows, link) {
		if (!w->swc) {
			if (debugscroll)
				fprintf(stderr, "[scroll] window node with null swc\n");
			continue;
		}
		/* when scroll with moving window, dont scroll the moving window, it makes it all jittery and ew */
		if (hevel.chord.moving && w->swc == hevel.focused)
			continue;
		if (!swc_window_get_geometry(w->swc, &geometry))
			continue;
		if (debugscroll) {
			hevel.chord.scroll_last = w->swc;
			hevel.chord.scroll_last_geo = geometry;
			hevel.chord.scroll_last_step = step;
		}
		swc_window_set_position(w->swc, geometry.x, geometry.y + step);
	}

	hevel.chord.scroll_pending_px -= step;
	wl_event_source_timer_update(hevel.chord.scroll_timer, scrollms);
	return 0;
}

static void
axis(void *data, uint32_t time, uint32_t axis, int32_t value120)
{
	int32_t dy;

	(void)data;

	/* while moving a window swallow scroll events so they don't reach clients */
	if (hevel.chord.moving)
		return;

	if (!hevel.chord.scrolling) {
		if (debugscroll)
			fprintf(stderr, "[scroll] forward axis=%u value120=%d (not scrolling)\n", axis, value120);
		swc_pointer_send_axis(time, axis, value120);
		return;
	}

	if (axis != 0 || value120 == 0) {
		if (debugscroll)
			fprintf(stderr, "[scroll] forward axis=%u value120=%d (axis!=0 or stop)\n", axis, value120);
		swc_pointer_send_axis(time, axis, value120);
		return;
	}

	hevel.chord.scroll_cursor_dir = value120 < 0 ? -1 : 1;
	update_mode_cursor();

	/* preserve sub-step precision for dat smooth, smooth scrolling */
	int64_t dy_num = (int64_t)hevel.chord.scroll_rem + (int64_t)value120 * (int64_t)scrollpx;
	if (dy_num > INT32_MAX)
		dy_num = INT32_MAX;
	if (dy_num < INT32_MIN)
		dy_num = INT32_MIN;
	dy = (int32_t)dy_num / 120;
	hevel.chord.scroll_rem = (int32_t)dy_num % 120;

	if (dy == 0)
		return;

	if (debugscroll) {
		fprintf(stderr, "[scroll] axis time=%u value120=%d dy=%d rem=%d pending=%d timer=%p\n",
		        time, value120, dy, hevel.chord.scroll_rem, hevel.chord.scroll_pending_px,
		        (void *)hevel.chord.scroll_timer);
	}

	int64_t pending = (int64_t)hevel.chord.scroll_pending_px + (int64_t)dy;
	if (pending > 100000 || pending < -100000) {
		if (debugscroll)
			fprintf(stderr, "[scroll] pending overflow %" PRId64 "; resetting\n", pending);
		scroll_stop();
		return;
	}
	hevel.chord.scroll_pending_px = (int32_t)pending;
	if (!hevel.chord.scroll_timer)
		hevel.chord.scroll_timer = wl_event_loop_add_timer(hevel.evloop, scroll_tick, NULL);
	if (hevel.chord.scroll_timer) {
		wl_event_source_timer_update(hevel.chord.scroll_timer, 1);
	} else if (debugscroll) {
		fprintf(stderr, "[scroll] failed to create timer; falling back to immediate\n");
		hevel.chord.scroll_pending_px = 0;
		hevel.chord.scroll_rem = 0;
	}
}

static void
windowdestroy(void *data)
{
	struct window *w = data;
	if (hevel.chord.scroll_last == w->swc)
		hevel.chord.scroll_last = NULL;
	if(hevel.focused == w->swc)
		focus_window(NULL, "destroy");
	wl_list_remove(&w->link);
	free(w);
}

static void
windowappidchanged(void *data)
{
	struct window *w = data;
	struct swc_rectangle geometry;
	bool is_select = hevel.chord.spawn.pending
	              && w->swc->app_id
	              && strcmp(w->swc->app_id, select_term_app_id) == 0;

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
	              && strcmp(swc->app_id, select_term_app_id) == 0;

	w = malloc(sizeof(*w));
	if(!w)
		return;
	w->swc = swc;
	wl_list_insert(&hevel.windows, &w->link);
	swc_window_set_handler(swc, &windowhandler, w);
	swc_window_set_stacked(swc);
	swc_window_set_border(swc, inner_border_color_inactive, inner_border_width, outer_border_color_inactive, outer_border_width);
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
	bool is_lr;
	bool is_chord_button;

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
		hevel.chord.middle = pressed;
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

	is_lr = (b == BTN_LEFT || b == BTN_RIGHT);
	is_chord_button = (is_lr || b == BTN_MIDDLE);
	
	if (b == BTN_LEFT && !pressed && hevel.chord.killing) {
		if (cursor_position(&x, &y)) {
			struct swc_window *target = swc_window_at(x, y);

			if (target)
				swc_window_close(target);
		}
		hevel.chord.killing = false;
		update_mode_cursor();
		if (!hevel.chord.left && !hevel.chord.middle && !hevel.chord.right)
			hevel.chord.activated = false;
		return;
	}

	if (b == BTN_LEFT && pressed && was_right && !hevel.chord.activated) {
		click_cancel();
		stop_select();
		hevel.chord.activated = true;
		hevel.chord.killing = true;
		update_mode_cursor();
		return;
	}
	
	if (b == BTN_MIDDLE && pressed && was_right && !hevel.chord.activated) {
		click_cancel();
		stop_select();
		hevel.chord.activated = true;
		hevel.chord.scrolling = true;
		hevel.chord.scroll_cursor_dir = -1;
		update_mode_cursor();
		scroll_stop();
		if (debugscroll)
			fprintf(stderr, "[scroll] start\n");
		return;
	}

	if (b == BTN_MIDDLE && !pressed && was_left && ! hevel.chord.activated && !hevel.chord.selecting) {
		click_cancel();
		stop_select();
		hevel.chord.activated = true;
		hevel.chord.moving = true;
		update_mode_cursor();

		/* get starting pos to be used for easing calculation*/
		if(hevel.focused && cursor_position(&x, &y)){
			struct swc_rectangle geometry;
			if(swc_window_get_geometry(hevel.focused, &geometry)){
				hevel.chord.move_start_win_x = geometry.x;
				hevel.chord.move_start_win_y = geometry.y;
				hevel.chord.move_start_cursor_x = x;
				hevel.chord.move_start_cursor_y = y;
			}
		}

		/* auto-scroll timer for scroll durin win move */
		if(!hevel.chord.move_scroll_timer)
			hevel.chord.move_scroll_timer = wl_event_loop_add_timer(hevel.evloop, move_scroll_tick, NULL);
		if(hevel.chord.move_scroll_timer)
			wl_event_source_timer_update(hevel.chord.move_scroll_timer, 16);

		return;
	}

	if (b == BTN_LEFT && !pressed && hevel.chord.moving == true) {
		hevel.chord.moving = false;
		update_mode_cursor();

		/* stop timer */
		if(hevel.chord.move_scroll_timer){
			wl_event_source_remove(hevel.chord.move_scroll_timer);
			hevel.chord.move_scroll_timer = NULL;
		}

		if (!hevel.chord.left && !hevel.chord.middle && !hevel.chord.right)
			hevel.chord.activated = false;

		return;
	}

	if (b == BTN_MIDDLE && !pressed && was_right && ! hevel.chord.activated && !hevel.chord.selecting) {
		click_cancel();
		stop_select();
		hevel.chord.activated = true;
		hevel.chord.resize = true;
		update_mode_cursor();

		if (hevel.focused)
			/* bottom right */
			swc_window_begin_resize(hevel.focused, SWC_WINDOW_EDGE_RIGHT | SWC_WINDOW_EDGE_BOTTOM);

		return;
	}

	if (b == BTN_RIGHT && !pressed && hevel.chord.resize == true) {
		hevel.chord.resize = false;
		update_mode_cursor();

		if (hevel.focused)
			swc_window_end_resize(hevel.focused);

		if (!hevel.chord.left && !hevel.chord.middle && !hevel.chord.right)
			hevel.chord.activated = false;

		return;
	}
	
	if (b == BTN_MIDDLE && !pressed && hevel.chord.scrolling) {
		return;
	}

	if (pressed && is_lr && !hevel.chord.selecting) {
		bool other_down = (b == BTN_LEFT) ? was_right : was_left;

		/* stop auto-scrolling on any clics */
		if (hevel.chord.auto_scrolling) {
			hevel.chord.auto_scrolling = false;
			scroll_stop();
		}

		if (!other_down && cursor_position(&x, &y)) {
			struct swc_window *target = swc_window_at(x, y);

			if (target)
				focus_window(target, "click");
		}
	}

	if(hevel.chord.left && hevel.chord.right && !hevel.chord.activated){
		click_cancel();
		hevel.chord.activated = true;
		if(cursor_position(&x, &y)){
			hevel.chord.selecting = true;
			update_mode_cursor();
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
	if(is_chord_button && hevel.chord.activated && !hevel.chord.selecting){
		bool was_scrolling = hevel.chord.scrolling;
		if (!hevel.chord.right)
			hevel.chord.scrolling = false;
		if (was_scrolling && !hevel.chord.scrolling)
			update_mode_cursor();
		if (!hevel.chord.scrolling) {
			if (debugscroll)
				fprintf(stderr, "[scroll] stop\n");
			scroll_stop();
		}
		if(!hevel.chord.left && !hevel.chord.middle && !hevel.chord.right)
			hevel.chord.activated = false;
		return;
	}

	if (b == BTN_MIDDLE) {
		if (hevel.chord.moving)
			return;
		swc_pointer_send_button(time, b, state);
		return;
	}

	/* pass normal clicks through to clients */
	if(is_lr && pressed && !hevel.chord.selecting){
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

	if(is_lr && !pressed && !hevel.chord.selecting){
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
		uint32_t bw = outer_border_width + inner_border_width;

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
		spawn_term_select(&geometry);
		printf("spawned terminal at %d,%d %ux%u\n", geometry.x, geometry.y, geometry.width, geometry.height);
	}

	if(!is_lr){
		swc_pointer_send_button(time, b, state);
		return;
	}

	if(!hevel.chord.left && !hevel.chord.middle && !hevel.chord.right)
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
	wl_list_init(&scrollpos_resources);

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
	
	wl_global_create(hevel.display, &hevel_scroll_interface, 1, NULL, bind_scrollpos);

	maybe_enable_nein_cursor_theme();
	
	#ifdef USE_IMAGE
		swc_wallpaper_init(imgpath);
	#endif

	#ifndef USE_IMAGE
		swc_wallpaper_color_set(background_color);
	#endif

	swc_add_binding(SWC_BINDING_KEY, SWC_MOD_LOGO | SWC_MOD_SHIFT,
	                XKB_KEY_q, quit, NULL);
	swc_add_binding(SWC_BINDING_BUTTON, SWC_MOD_ANY, BTN_LEFT, button, NULL);
	swc_add_binding(SWC_BINDING_BUTTON, SWC_MOD_ANY, BTN_MIDDLE, button, NULL);
	swc_add_binding(SWC_BINDING_BUTTON, SWC_MOD_ANY, BTN_RIGHT, button, NULL);
	if (swc_add_axis_binding(SWC_MOD_ANY, 0, axis, NULL) < 0)
		fprintf(stderr, "cannot bind vertical scroll axis\n");
	if (swc_add_axis_binding(SWC_MOD_ANY, 1, axis, NULL) < 0)
		fprintf(stderr, "cannot bind horizontal scroll axis\n");

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
