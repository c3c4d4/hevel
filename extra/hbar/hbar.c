#define _POSIX_C_SOURCE 200809L

/* velox: clients/status_bar.c
 *
 * Copyright (c) 2010, 2013, 2014 Michael Forney <mforney@mforney.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* note from dalem: totally janky hack, see readme todo */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <sys/signalfd.h>
#include <time.h>
#include <wayland-client.h>
#include <wld/wayland.h>
#include <wld/wld.h>

#include <swc-client-protocol.h>
#include "hevel-client-protocol.h"

enum align {
	ALIGN_LEFT,
	ALIGN_CENTER,
	ALIGN_RIGHT,
};

struct item {
	const struct item_interface *interface;
	const struct item_data *data;
	struct wl_list link;
};

struct item_data {
	uint32_t width;
};

struct text_item_data {
	struct item_data base;
	const char *text;
};

struct status_bar {
	struct wl_surface *surface;
	struct swc_panel *panel;

	struct wld_surface *wld_surface;
	uint32_t width, height;

	struct wl_list items[3];
};

struct item_interface {
	void (*draw)(struct status_bar *status_bar, struct item *item, uint32_t x, uint32_t y);
};

struct style {
	uint32_t fg, bg;
};

struct screen {
	struct swc_screen *swc;
	struct status_bar status_bar;
	struct wl_list link;
};

/* Wayland listeners */
static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *implementation, uint32_t version);
static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name);

static void panel_docked(void *data, struct swc_panel *panel, uint32_t length);
static void hevel_bar_scroll(void *data, struct hevel_scroll *hscroll, int32_t pos);

/* Item interfaces */
struct scroll {
	struct hevel_scroll *scroll;
	int32_t pos;
};

static struct scroll hevel;

static void text_draw(struct status_bar *status_bar, struct item *item, uint32_t x, uint32_t y);

static struct wl_display *display;
static struct wl_registry *registry;
static struct wl_compositor *compositor;
static struct swc_panel_manager *panel_manager;

static struct wl_list screens;

static struct {
	struct wld_context *context;
	struct wld_renderer *renderer;
	struct wld_font_context *font_context;
	struct wld_font *font;
} wld;

static const struct wl_registry_listener registry_listener = {
	.global = &registry_global,
	.global_remove = &registry_global_remove
};

static const struct swc_panel_listener panel_listener = {
	.docked = &panel_docked
};

static const struct item_interface text_interface = {
	.draw = &text_draw
};

static const struct hevel_scroll_listener hevel_scroll_listener = {
	.get_pos = hevel_bar_scroll,
};

/* Configuration parameters */
static const int spacing = 1;
static const char *const font_name = "Terminus:pixelsize=14";
static const struct style normal = { .bg = 0xff1a1a1a, .fg = 0xff999999 };

static char scroll_text[32];
static struct text_item_data scroll_data = {.text = scroll_text };

static timer_t timer;
static bool running, need_draw;
static char clock_text[32];
static struct text_item_data clock_data = {.text = clock_text };

static void __attribute__((noreturn)) die(const char *const format, ...)
{
	va_list args;

	va_start(args, format);
	fputs("FATAL: ", stderr);
	vfprintf(stderr, format, args);
	fputc('\n', stderr);
	va_end(args);
	exit(EXIT_FAILURE);
}

static void *
xmalloc(size_t size)
{
	void *data;

	if (!(data = malloc(size)))
		die("Allocation failed");
	return data;
}

static struct item *
item_new(const struct item_interface *interface, const struct item_data *data)
{
	struct item *item;

	if (!(item = malloc(sizeof(*item))))
		die("Failed to allocate item");

	item->interface = interface;
	item->data = data;

	return item;
}

static void
update_text_item_data(struct text_item_data *data)
{
	struct wld_extents extents;

	wld_font_text_extents(wld.font, data->text, &extents);
	data->base.width = extents.advance + spacing;
	need_draw = true;
}

/* Wayland event handlers */
static void
registry_global(void *data, struct wl_registry *registry,
                uint32_t name, const char *interface, uint32_t version)
{
	if (strcmp(interface, "wl_compositor") == 0) {
		compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 3);
	} else if (strcmp(interface, "swc_panel_manager") == 0) {
		panel_manager = wl_registry_bind(registry, name, &swc_panel_manager_interface, 1);
	} else if (strcmp(interface, "swc_screen") == 0) {
		struct screen *screen;

		screen = xmalloc(sizeof(*screen));
		screen->swc = wl_registry_bind(registry, name, &swc_screen_interface, 1);
		if (!screen->swc)
			die("Failed to bind swc_screen");
		wl_list_insert(screens.prev, &screen->link);
	} else if(strcmp(interface, "hevel_scroll") == 0) {
		hevel.scroll = wl_registry_bind(registry, name, &hevel_scroll_interface, 1);
		hevel.pos = 0;
		hevel_scroll_add_listener(hevel.scroll, &hevel_scroll_listener, NULL);
	}
}

static void
registry_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
}

static void
panel_docked(void *data, struct swc_panel *panel, uint32_t length)
{
	struct status_bar *bar = data;

	bar->width = length / 4;
	bar->height = wld.font->height + 2;
	bar->wld_surface = wld_wayland_create_surface(wld.context, bar->width, bar->height,
	                                              WLD_FORMAT_XRGB8888, 0, bar->surface);
}

/* Item implementations */
void
text_draw(struct status_bar *bar, struct item *item, uint32_t x, uint32_t y)
{
	struct text_item_data *data = (void *)item->data;

	wld_draw_text(wld.renderer, wld.font, normal.fg, x, y + wld.font->ascent + 1, data->text, -1, NULL);
}

static void
draw(struct status_bar *bar)
{
	struct item *item;
	uint32_t start_x[3] = {
		0, (bar->width / 4), bar->width
	};
	uint32_t x;

	wld_set_target_surface(wld.renderer, bar->wld_surface);
	wld_fill_rectangle(wld.renderer, normal.bg, 0, 0, bar->width, bar->height);

	wl_list_for_each (item, &bar->items[ALIGN_CENTER], link)
		start_x[ALIGN_CENTER] -= item->data->width / 2;

	wl_list_for_each (item, &bar->items[ALIGN_RIGHT], link)
		start_x[ALIGN_RIGHT] -= item->data->width;

	x = start_x[ALIGN_LEFT];
	wl_list_for_each (item, &bar->items[ALIGN_LEFT], link) {
		item->interface->draw(bar, item, x, 0);
		x += item->data->width;
	}

	x = start_x[ALIGN_CENTER];
	wl_list_for_each (item, &bar->items[ALIGN_CENTER], link) {
		item->interface->draw(bar, item, x, 0);
		x += item->data->width;
	}

	x = start_x[ALIGN_RIGHT];
	wl_list_for_each (item, &bar->items[ALIGN_RIGHT], link) {
		item->interface->draw(bar, item, x, 0);
		x += item->data->width;
	}

	wl_surface_damage(bar->surface, 0, 0, bar->width, bar->height);
	wld_flush(wld.renderer);
	wld_swap(bar->wld_surface);
}

static void
hevel_bar_scroll(void *data, struct hevel_scroll *hscroll, int32_t pos)
{
	(void)data;
	(void)hscroll;

	hevel.pos = pos;
	snprintf(scroll_text, sizeof(scroll_text), "pos: %d", pos);
	update_text_item_data(&scroll_data);
}

static void
setup(void)
{
	struct status_bar *status_bar;
	struct screen *screen;
	struct wl_list *items;
	struct item *item;

	fprintf(stderr, "status bar: Initializing...");

	wl_list_init(&screens);

	if (timer_create(CLOCK_MONOTONIC, NULL, &timer) != 0)
		die("Failed to create timer: %s", strerror(errno));

	if (!(display = wl_display_connect(NULL)))
		die("Failed to connect to display");

	if (!(registry = wl_display_get_registry(display)))
		die("Failed to get registry");

	wl_registry_add_listener(registry, &registry_listener, NULL);

	/* Wait for globals. */
	wl_display_roundtrip(display);

	if (!compositor || !panel_manager || !hevel.scroll) {
		die("Missing required globals: wl_compositor, swc_panel_manager, hevel_scroll");
	}

	wld.context = wld_wayland_create_context(display, WLD_ANY);
	if (!wld.context)
		die("Failed to create WLD context");

	wld.renderer = wld_create_renderer(wld.context);
	if (!wld.renderer)
		die("Failed to create WLD renderer");

	/* Font */
	wld.font_context = wld_font_create_context();
	if (!wld.font_context)
		die("Failed to create WLD font context");
	wld.font = wld_font_open_name(wld.font_context, font_name);
	if (!wld.font)
		die("Failed to open font");

	/* Create the panels */
	wl_list_for_each (screen, &screens, link) {

		status_bar = &screen->status_bar;
		status_bar->surface = wl_compositor_create_surface(compositor);
		status_bar->panel = swc_panel_manager_create_panel(panel_manager, status_bar->surface);
		swc_panel_add_listener(status_bar->panel, &panel_listener, status_bar);
		swc_panel_dock(status_bar->panel, SWC_PANEL_EDGE_TOP, screen->swc, false);

		/* Add items */
		items = &screen->status_bar.items[ALIGN_LEFT];
		wl_list_init(items);

                items = &screen->status_bar.items[ALIGN_LEFT];
                wl_list_init(items);

                items = &screen->status_bar.items[ALIGN_CENTER];
                wl_list_init(items);

                items = &screen->status_bar.items[ALIGN_RIGHT];
                wl_list_init(items);

		/* Clock */
		item = item_new(&text_interface, &clock_data.base);
		wl_list_insert(items, &item->link);
		
		items = &screen->status_bar.items[ALIGN_LEFT];
		item = item_new(&text_interface, &scroll_data.base);
		wl_list_insert(items, &item->link);
	}

	/* Wait for dock notifications. */
	wl_display_roundtrip(display);

	wl_list_for_each (screen, &screens, link) {
		if (!screen->status_bar.wld_surface)
			die("");
		swc_panel_set_strut(screen->status_bar.panel, screen->status_bar.height, 0, screen->status_bar.width);
	}

	fprintf(stderr, "done\n");

	wl_display_flush(display);
}

static void
run(void)
{
	sigset_t signals;
	struct itimerspec timer_value = {
		.it_interval = { 1, 0 },
		.it_value = { 0, 1 }
	};
	struct pollfd fds[2];
	struct screen *screen;

	sigemptyset(&signals);
	sigaddset(&signals, SIGALRM);
	sigprocmask(SIG_BLOCK, &signals, NULL);

	fds[0].fd = wl_display_get_fd(display);
	fds[0].events = POLLIN;
	fds[1].fd = signalfd(-1, &signals, SFD_CLOEXEC);
	fds[1].events = POLLIN;

	timer_settime(timer, 0, &timer_value, NULL);
	running = true;

	while (true) {
		if (poll(fds, sizeof(fds) / sizeof(fds[0]), -1) == -1)
			break;

		if (fds[0].revents & POLLIN) {
			if (wl_display_dispatch(display) == -1) {
				fprintf(stderr, "Wayland dispatch error: %s\n",
				        strerror(wl_display_get_error(display)));
				break;
			}
		}
		if (fds[1].revents & POLLIN) {
			time_t raw_time = time(NULL);
			struct tm *local_time = localtime(&raw_time);

			sigwaitinfo(&signals, NULL);

			strftime(clock_text, sizeof(clock_text), "%A %T %F", local_time);
			update_text_item_data(&clock_data);
		}

		if (need_draw) {
			wl_list_for_each (screen, &screens, link)
				draw(&screen->status_bar);
			need_draw = false;
		}

		wl_display_flush(display);
	}
}

int
main(int argc, char *argv[])
{
	setup();
	run();

	return EXIT_SUCCESS;
}
