/* swcsnap: simple screenshots for our custom swc
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wayland-client.h>
#include "swc/protocol/swc_snap-client-protocol.h"

static struct swc_snap *snap_manager = NULL;

static void
registry_global(void *data, struct wl_registry *registry, uint32_t name,
                const char *interface, uint32_t version)
{
	(void)data;
	(void)version;

	if (strcmp(interface, swc_snap_interface.name) == 0) {
		snap_manager = wl_registry_bind(registry, name, &swc_snap_interface, 1);
	}
}

static void
registry_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
	(void)data;
	(void)registry;
	(void)name;
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};

int
main(int argc, char *argv[])
{
	struct wl_display *display;
	struct wl_registry *registry;

	(void)argc;
	(void)argv;

	display = wl_display_connect(NULL);
	if (!display) {
		fprintf(stderr, "Failed to connect to Wayland display\n");
		return 1;
	}

	registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_roundtrip(display);

	if (!snap_manager) {
		fprintf(stderr, "swc_snap protocol not available\n");
		wl_registry_destroy(registry);
		wl_display_disconnect(display);
		return 1;
	}

	swc_snap_capture(snap_manager, STDOUT_FILENO);

	/* Wait for compositor to process the request */
	wl_display_roundtrip(display);

	/* Give compositor time to write */
	wl_display_roundtrip(display);

	/* cleanup  */
	swc_snap_destroy(snap_manager);
	wl_registry_destroy(registry);
	wl_display_disconnect(display);

	return 0;
}
