hevel 
-----

"Make the user interface invisible"

hevel is a scrollable, floating window manager for Wayland that uses mouse
chords for all commands.

Its design is inspired by ideas from Rob Pike's 1988 paper, "Window Systems 
Should be Transparent", taken to their logical extremes. In this sense, hevel
is a modernization of mouse-driven Unix and Plan 9 window systems such as mux,
8½, and rio.

Unlike those systems, hevel has no menus and is not limited to a single
screen of space. Instead, the desktop is an infinite plane:
windows can be created anywhere, and the view can be freely scrolled thru (vertically, or in all axis).

hevel is implemented using the [neuswc](https://git.sr.ht/~shrub900/neuswc) library.

hevel is the flagship window manager designed for use with 
[dérive linux](https://derivelinux.org).

**WARNING**: This is experimental software. Use at your own risk.

Commands 
--------

Commands are issued using mouse chords: combinations of mouse buttons pressed 
in sequence.

Mouse buttons are referred to as follows:
- 1: left click
- 2: middle click (scroll wheel)
- 3: right click.

Here are the commands:

- 1 → 3 → drag → release

  Create a new terminal in the dragged rectangle.
  
- 3 → 1 → move mouse over target window → release

  Kill the target window.
   
- 3 → 2 → release 2, keep holding the scroll wheel

  Scroll vertically in vertical mode, and drag the cursor in drag mode.

- 2 → 3 → release 2 over a window, and then drag with 3

  Resize the window.

- 2 → 1 → release 2 over a window, then drag with 1

  Move the window. Dragging to the top or bottom of the screen begins 
  scrolling.

- 1 → 2

  User-configurable (see config.h).

Building
----- 

To build hevel, you will need the [wld](https://github.com/michaelforney/wld) 
library installed. Hevel requires the following dependencies:

- A C11-compatible compiler
- make
- pkg-config
- wayland-scanner, wayland-server, wayland-client
- wayland-server, wayland-client
- libinput, libdrm, pixman, xkbcommon
- wld
- libudev, xcb, xcb-composite, xcb-ewmh, xcb-icccm 

**NOTE**: The last dependencies are optional. They are required 
for the `ENABLE_LIBUDEV` and `ENABLE_XWAYLAND` options respectively 
in neuswc 

```
make
make install 
```

To run:

```
swc-launch hevel
```
Linux is the primary supported platform. NetBSD and FreeBSD also work, but may

require minor Makefile adjustments. Depending on your setup, you may want to 
tweak neuswc itself

hevel-specific configuration is done at compile time via config.h.
