PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

CFLAGS = -O2 -std=c11 -Wall -Wextra
CFLAGS += -I$(PREFIX)/include
CFLAGS += `pkg-config --cflags wayland-server libinput pixman-1 xkbcommon libdrm`

LDFLAGS = -L$(PREFIX)/lib -Wl,-rpath,$(PREFIX)/lib
LDLIBS = -lswc -lwld
LDLIBS += `pkg-config --libs wayland-server libinput pixman-1 xkbcommon libdrm libudev xcb xcb-composite xcb-ewmh xcb-icccm`

hevel: hevel.o
	$(CC) $(LDFLAGS) -o hevel hevel.o $(LDLIBS)

hevel.o: hevel.c
	$(CC) $(CFLAGS) -c hevel.c

clean:
	rm -f hevel hevel.o

install: hevel
	install -D -m 755 hevel $(DESTDIR)$(BINDIR)/hevel

.PHONY: clean install
