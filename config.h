static const uint32_t outer_border_color_inactive = 0xff525252;
static const uint32_t inner_border_color_inactive = 0xff85b3d6;

static const uint32_t outer_border_color_active = 0xff222222;
static const uint32_t inner_border_color_active = 0xff285577;

static const uint32_t outer_border_width = 2;
static const uint32_t inner_border_width = 2;

static const uint32_t select_box_color = 0xffffffff;
static const uint32_t select_box_border = 2;

static const char *const select_term_app_id = "hevel-select";
static const char *const term = "havoc";

static const int chord_click_timeout_ms = 125;

static const int32_t move_scroll_edge_threshold = 80;
static const int32_t move_scroll_speed = 8;
static const float move_ease_factor = 0.37f;

/* cursor themes:
 * - "swc"  : use swc's built-in cursor ,client cursors allowed, no per-chord cursor
 * - "nein" : use the plan 9 cursor set, client cursors blocked, per chord cursors
 */
static const char *const cursor_theme = "nein";
