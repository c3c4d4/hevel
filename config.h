static const uint32_t background_color = 0xff777777;

static const uint32_t outer_border_color_inactive = 0xffffffea;
static const uint32_t inner_border_color_inactive = 0xffddbd8c;

static const uint32_t outer_border_color_active = 0xffffffea;
static const uint32_t inner_border_color_active = 0xffc99043;

static const uint32_t outer_border_width = 4;
static const uint32_t inner_border_width = 4;

static const uint32_t select_box_color = 0xffffffff;
static const uint32_t select_box_border = 2;

/* cursor themes:
 * - "swc"  : use swc's built-in cursor, client cursors allowed, no per-chord cursor
 * - "nein" : use the plan 9 cursor set, client cursors blocked, per chord cursors
 */
static const char *const cursor_theme = "nein";

static const char *const select_term_app_id = "st-wl-256color";
static const char *const term = "st-wl";

/* a flag for your terminal emulator to setup a windowid 
 * - for havoc: -i
 * - for st-wl: -w
 * - for everything else: idk
 */
static const char *const term_flag = "-w";

/* gui programs take over the geometry of the terminal, broken for xwayland */
static const bool enable_terminal_spawning = true;

/* define a list of terminals that you use */
static const char *const terminal_app_ids[] = {
	"havoc",
	"st-wl",
	"alacritty",
	NULL
};

static const int chord_click_timeout_ms = 250;

static const int32_t move_scroll_edge_threshold = 80;
static const int32_t move_scroll_speed = 16;
static const float move_ease_factor = 0.30f;

/* scroll chord mode:
 * - true  : drag mouse to scroll in any direction
 * - false : use scroll wheel for vertical scrolling only
 */
static const bool scroll_drag_mode = true;

/* whether or not to center the window.
 * in drag mode, it centers on both axis
 * otherwise on the vertical axis
 */
static bool focus_center = true;

/*
 * enable zoom feature:
 * - when enabled: scroll wheel controls zoom when in drag scroll mode
 * broken for multiple monitors
 */
static const bool enable_zoom = true;

/* customizable 2-1 chord
 * avaliable options:
 * - STICKY: make window not move when scroll
 * - FULLSCREEN: make a window take entire screen
 * - JUMP: switch focus to the closest window
 */
#define JUMP
