#define USE_IMAGE true

#ifdef USE_IMAGE
	static char* imgpath = "/home/srb/Downloads/2.png";
#endif 

static const uint32_t background_color = 0xFF787878;

static const uint32_t outer_border_color_inactive = 0xffffffea;
static const uint32_t inner_border_color_inactive = 0xffddbd8c;

static const uint32_t outer_border_color_active = 0xffffffea;
static const uint32_t inner_border_color_active = 0xffc99043;

static const uint32_t outer_border_width = 4;
static const uint32_t inner_border_width = 4;

static const uint32_t select_box_color = 0xffffffff;
static const uint32_t select_box_border = 2;

static const char *const select_term_app_id = "hevel-select";
static const char *const term = "havoc";

static const int chord_click_timeout_ms = 125;

static const int32_t move_scroll_edge_threshold = 80;
static const int32_t move_scroll_speed = 16;
static const float move_ease_factor = 0.30f;

/* cursor themes:
 * - "swc"  : use swc's built-in cursor ,client cursors allowed, no per-chord cursor
 * - "nein" : use the plan 9 cursor set, client cursors blocked, per chord cursors
 */
static const char *const cursor_theme = "nein";

static const bool enable_terminal_spawning = true;

/* scroll chord mode:
 * - true  : drag mouse to scroll in any direction
 * - false : use scroll wheel for vertical scrolling only
 */
static const bool scroll_drag_mode = true;

static const char *const terminal_app_ids[] = {
	"havoc",
	"alacritty",
	NULL
};
