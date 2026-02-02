PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

PROTO_DIR = protocol

PROTO_HEVEL = $(PROTO_DIR)/hevel.xml
PROTO_HEVEL_SERVER_C = $(PROTO_DIR)/hevel-server.c
PROTO_HEVEL_SERVER_H = $(PROTO_DIR)/hevel-server-protocol.h
PROTO_HEVEL_CLIENT_H = $(PROTO_DIR)/hevel-client-protocol.h
PROTO_HEVEL_CLIENT_C = $(PROTO_DIR)/hevel-client-protocol.c
PROTO_HEVEL_SERVER_O = $(PROTO_DIR)/hevel-server.o
PROTO_HEVEL_CLIENT_O = $(PROTO_DIR)/hevel-client-protocol.o

CFLAGS = -O2 -std=c11 -Wall -Wextra
CFLAGS += -I$(PREFIX)/include
CFLAGS += -I$(PROTO_DIR)
CFLAGS += `pkg-config --cflags swc wayland-server libinput pixman-1 xkbcommon libdrm wld`

LDFLAGS = -L$(PREFIX)/lib -Wl,-rpath,$(PREFIX)/lib
LDLIBS += `pkg-config --libs swc wayland-server libinput pixman-1 xkbcommon libdrm libudev xcb xcb-composite xcb-ewmh xcb-icccm wld`

SNAP_CLIENT_CFLAGS = -O2 -std=c11 -Wall -Wextra
SNAP_CLIENT_CFLAGS += `pkg-config --cflags swc wayland-client libinput pixman-1 xkbcommon libdrm wld` 
SNAP_CLIENT_LDLIBS = `pkg-config --libs swc wayland-client libinput pixman-1 xkbcommon libdrm libudev xcb xcb-composite xcb-ewmh xcb-icccm wld`
SNAP_C = extra/swcsnap/swcsnap.c

HBAR_C = extra/hbar/hbar.c
HBAR_O = extra/hbar/hbar.o
HBAR_CFLAGS = -O2 -std=c11 -Wall -Wextra -Wno-unused-parameter
HBAR_CFLAGS += `pkg-config --cflags swc wayland-client libinput pixman-1 xkbcommon libdrm wld` 
HBAR_CFLAGS += -I$(PROTO_DIR)
HBAR_LDLIBS = `pkg-config --libs swc wayland-client libinput pixman-1 xkbcommon libdrm libudev xcb xcb-composite xcb-ewmh xcb-icccm wld`

all: hevel swcsnap hbar

hevel: hevel.o $(PROTO_HEVEL_SERVER_O)
	$(CC) $(LDFLAGS) -o hevel hevel.o $(PROTO_HEVEL_SERVER_O) $(LDLIBS)

hevel.o: hevel.c $(PROTO_HEVEL_SERVER_H)
	$(CC) $(CFLAGS) -c hevel.c

swcsnap: swcsnap.o 
	$(CC) $(LDFLAGS) -o swcsnap swcsnap.o $(SNAP_CLIENT_LDLIBS)

swcsnap.o: $(SNAP_C)
	$(CC) $(SNAP_CLIENT_CFLAGS) -c $(SNAP_C)

$(PROTO_HEVEL_SERVER_O): $(PROTO_HEVEL_SERVER_C) $(PROTO_HEVEL_SERVER_H)
	$(CC) $(CFLAGS) -c $(PROTO_HEVEL_SERVER_C) -o $(PROTO_HEVEL_SERVER_O)

$(PROTO_HEVEL_SERVER_H) $(PROTO_HEVEL_CLIENT_H) $(PROTO_HEVEL_SERVER_C) $(PROTO_HEVEL_CLIENT_C): $(PROTO_HEVEL)
	wayland-scanner server-header $(PROTO_HEVEL) $(PROTO_HEVEL_SERVER_H)
	wayland-scanner client-header $(PROTO_HEVEL) $(PROTO_HEVEL_CLIENT_H)
	wayland-scanner private-code $(PROTO_HEVEL) $(PROTO_HEVEL_SERVER_C)
	wayland-scanner public-code $(PROTO_HEVEL) $(PROTO_HEVEL_CLIENT_C)

$(PROTO_HEVEL_CLIENT_O): $(PROTO_HEVEL_CLIENT_C) $(PROTO_HEVEL_CLIENT_H)
	$(CC) $(HBAR_CFLAGS) -c $(PROTO_HEVEL_CLIENT_C) -o $(PROTO_HEVEL_CLIENT_O)

hbar: $(HBAR_O) $(PROTO_HEVEL_CLIENT_O)
	$(CC) $(LDFLAGS) -o hbar $(HBAR_O) $(PROTO_HEVEL_CLIENT_O) $(HBAR_LDLIBS)

$(HBAR_O): $(PROTO_HEVEL_CLIENT_O)
	$(CC) $(HBAR_CFLAGS) -c $(HBAR_C) -o $(HBAR_O)

clean:
	rm -f hevel hevel.o
	rm -f $(PROTO_HEVEL_SERVER_H) $(PROTO_HEVEL_CLIENT_H) $(PROTO_HEVEL_SERVER_C) $(PROTO_HEVEL_CLIENT_C) $(PROTO_HEVEL_SERVER_O) $(PROTO_HEVEL_CLIENT_O)
	rm -f swcsnap swcsnap.o
	rm -f hbar extra/hbar/hbar.o
	$(MAKE) -C $(SWC_DIR) clean

install: hevel
	install -D -m 755 hevel $(DESTDIR)$(BINDIR)/hevel
	install -D -m 755 swcsnap $(DESTDIR)$(BINDIR)/swcsnap
	install -D -m 755 hbar $(DESTDIR)$(BINDIR)/hbar
	
.PHONY: clean install FORCE
