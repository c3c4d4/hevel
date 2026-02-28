PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

PROTO_DIR = protocol

PROTO_MURA = $(PROTO_DIR)/mura.xml
PROTO_MURA_SERVER_C = $(PROTO_DIR)/mura-server.c
PROTO_MURA_SERVER_H = $(PROTO_DIR)/mura-server-protocol.h
PROTO_MURA_CLIENT_H = $(PROTO_DIR)/mura-client-protocol.h
PROTO_MURA_CLIENT_C = $(PROTO_DIR)/mura-client-protocol.c
PROTO_MURA_SERVER_O = $(PROTO_DIR)/mura-server.o
PROTO_MURA_CLIENT_O = $(PROTO_DIR)/mura-client-protocol.o

CFLAGS = -O2 -std=c99 -Wall -Wextra
CFLAGS += -I$(PREFIX)/include
CFLAGS += -I$(PROTO_DIR)
CFLAGS += `pkg-config --cflags swc wayland-server libinput pixman-1 xkbcommon libdrm wld`

LDFLAGS = -L$(PREFIX)/lib -Wl,-rpath,$(PREFIX)/lib
LDLIBS += `pkg-config --libs swc wayland-server libinput pixman-1 xkbcommon libdrm libudev xcb xcb-composite xcb-ewmh xcb-icccm wld`

SNAP_CLIENT_CFLAGS = -O2 -std=c99 -Wall -Wextra
SNAP_CLIENT_CFLAGS += `pkg-config --cflags swc wayland-client libinput pixman-1 xkbcommon libdrm wld`
SNAP_CLIENT_LDLIBS = `pkg-config --libs swc wayland-client libinput pixman-1 xkbcommon libdrm libudev xcb xcb-composite xcb-ewmh xcb-icccm wld`
SNAP_C = extra/swcsnap/swcsnap.c

HBAR_C = extra/hbar/hbar.c
HBAR_O = extra/hbar/hbar.o
HBAR_CFLAGS = -O2 -std=c99 -Wall -Wextra -Wno-unused-parameter
HBAR_CFLAGS += `pkg-config --cflags swc wayland-client libinput pixman-1 xkbcommon libdrm wld`
HBAR_CFLAGS += -I$(PROTO_DIR)
HBAR_LDLIBS = `pkg-config --libs swc wayland-client libinput pixman-1 xkbcommon libdrm libudev xcb xcb-composite xcb-ewmh xcb-icccm wld`

all: mura swcsnap hbar

mura: mura.o $(PROTO_MURA_SERVER_O)
	$(CC) $(LDFLAGS) -o mura mura.o $(PROTO_MURA_SERVER_O) $(LDLIBS)

mura.o: mura.c $(PROTO_MURA_SERVER_H)
	$(CC) $(CFLAGS) -c mura.c

swcsnap: swcsnap.o
	$(CC) $(LDFLAGS) -o swcsnap swcsnap.o $(SNAP_CLIENT_LDLIBS)

swcsnap.o: $(SNAP_C)
	$(CC) $(SNAP_CLIENT_CFLAGS) -c $(SNAP_C)

$(PROTO_MURA_SERVER_O): $(PROTO_MURA_SERVER_C) $(PROTO_MURA_SERVER_H)
	$(CC) $(CFLAGS) -c $(PROTO_MURA_SERVER_C) -o $(PROTO_MURA_SERVER_O)

$(PROTO_MURA_SERVER_H) $(PROTO_MURA_CLIENT_H) $(PROTO_MURA_SERVER_C) $(PROTO_MURA_CLIENT_C): $(PROTO_MURA)
	wayland-scanner server-header $(PROTO_MURA) $(PROTO_MURA_SERVER_H)
	wayland-scanner client-header $(PROTO_MURA) $(PROTO_MURA_CLIENT_H)
	wayland-scanner private-code $(PROTO_MURA) $(PROTO_MURA_SERVER_C)
	wayland-scanner public-code $(PROTO_MURA) $(PROTO_MURA_CLIENT_C)

$(PROTO_MURA_CLIENT_O): $(PROTO_MURA_CLIENT_C) $(PROTO_MURA_CLIENT_H)
	$(CC) $(HBAR_CFLAGS) -c $(PROTO_MURA_CLIENT_C) -o $(PROTO_MURA_CLIENT_O)

hbar: $(HBAR_O) $(PROTO_MURA_CLIENT_O)
	$(CC) $(LDFLAGS) -o hbar $(HBAR_O) $(PROTO_MURA_CLIENT_O) $(HBAR_LDLIBS)

$(HBAR_O): $(PROTO_MURA_CLIENT_O)
	$(CC) $(HBAR_CFLAGS) -c $(HBAR_C) -o $(HBAR_O)

clean:
	rm -f mura mura.o
	rm -f $(PROTO_MURA_SERVER_H) $(PROTO_MURA_CLIENT_H) $(PROTO_MURA_SERVER_C) $(PROTO_MURA_CLIENT_C) $(PROTO_MURA_SERVER_O) $(PROTO_MURA_CLIENT_O)
	rm -f swcsnap swcsnap.o
	rm -f hbar extra/hbar/hbar.o

install: mura
	install -D -m 755 mura $(DESTDIR)$(BINDIR)/mura
	install -D -m 755 swcsnap $(DESTDIR)$(BINDIR)/swcsnap
	install -D -m 755 hbar $(DESTDIR)$(BINDIR)/hbar

.PHONY: clean install FORCE
