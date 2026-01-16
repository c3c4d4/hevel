hevel
-----

"make the user interface invisible"

hevel is a scrollable, floating window manager for wayland, that uses mouse chords for commands.

it's design is inspired by the ideas found in rob pike's 1988 paper, 'Window Systems Should be Transparent',
 taken to their logical extremes. As such, you could say it is a modernization of the line of mouse-driven
 unix/plan9 window systems, mux, 8½, and rio, all created by rob pike. however, it differs in the fact that
 there are no menus, and you are not limited to a single screen worth of space; you are able to infinitely
 scroll up and down, creating windows anywhere on the plane.

it is implemented using the wonderful swc library, with a fair few extensions.

hevel is the flagship window manager designed and implemented for use with [dérive linux](https://derivelinux.org)

commands
--------

commands are issued using mouse chords, meaning by pressing a combination of buttons on the mouse.
 left click will be reffered to as 1, clicking the scroll wheel as 2, and right click as 3.

- 1 -> 3 -> drag -> release -> creates a new terminal in the dragged box 
- 3 -> 1 -> move mouse on top of target window -> release -> kill target window
- 3 -> 2 -> release 2, and with your finger still on the scroll wheel, scroll up/down
- 2 -> 3 -> release 2 on top of a window, and then drag with 3 to resize said window
- 2 -> 1 -> release 2 on top of a window, and then drag with 1 to move said window. if you drag to the bottom/top of the screen, it will begin to scroll.

other
-----
you will need the michaelforney/wld library from github installed to build hevel.

to build:

```
make
make install
cd swc
make install
```

keep in mind this is EXPERIMENTAL SOFTWARE!

linux support is first-class, netbsd works too but you may have to twiddle with makefiles a bit, sorry.

configuration can be done with config.h at compile time.






