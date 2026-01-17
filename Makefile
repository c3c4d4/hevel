PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

SWC_DIR = swc
SWC_LIB = $(SWC_DIR)/libswc/libswc.a

PROTO_DIR = protocol

PROTO_HEVEL = $(PROTO_DIR)/hevel.xml
PROTO_HEVEL_SERVER_C = $(PROTO_DIR)/hevel-server.c
PROTO_HEVEL_SERVER_H = $(PROTO_DIR)/hevel-server-protocol.h
PROTO_HEVEL_CLIENT_H = $(PROTO_DIR)/hevel-client-protocol.h
PROTO_HEVEL_CLIENT_C = $(PROTO_DIR)/hevel-client-protocol.c
PROTO_HEVEL_SERVER_O = $(PROTO_DIR)/hevel-server.o
PROTO_HEVEL_CLIENT_O = $(PROTO_DIR)/hevel-client-protocol.o

CFLAGS = -O2 -std=c11 -Wall -Wextra
CFLAGS += -I$(SWC_DIR)/libswc
CFLAGS += -I$(PREFIX)/include
CFLAGS += -I$(PROTO_DIR)
CFLAGS += `pkg-config --cflags wayland-server libinput pixman-1 xkbcommon libdrm`

LDFLAGS = -L$(PREFIX)/lib -Wl,-rpath,$(PREFIX)/lib
LDLIBS = $(SWC_LIB) -lwld
LDLIBS += `pkg-config --libs wayland-server libinput pixman-1 xkbcommon libdrm libudev xcb xcb-composite xcb-ewmh xcb-icccm`

SNAP_CLIENT_CFLAGS = -O2 -std=c11 -Wall -Wextra
SNAP_CLIENT_CFLAGS += -I$(SWC_DIR)/protocol
SNAP_CLIENT_CFLAGS += `pkg-config --cflags wayland-client`
SNAP_CLIENT_LDLIBS = `pkg-config --libs wayland-client`
SNAP_C = extra/swcsnap/swcsnap.c

HBAR_C = extra/hbar/hbar.c
HBAR_O = extra/hbar/hbar.o
HBAR_CFLAGS = -O2 -std=c11 -Wall -Wextra -Wno-unused-parameter
HBAR_CFLAGS += `pkg-config --cflags wld wayland-client`
HBAR_CFLAGS += -I$(PROTO_DIR)
HBAR_CFLAGS += -I$(SWC_DIR)/protocol
HBAR_LDLIBS = `pkg-config --libs wld wayland-client`

all: hevel swcsnap hbar

hevel: $(SWC_LIB) hevel.o $(PROTO_HEVEL_SERVER_O)
	$(CC) $(LDFLAGS) -o hevel hevel.o $(PROTO_HEVEL_SERVER_O) $(LDLIBS)

hevel.o: hevel.c $(SWC_DIR)/libswc/swc.h $(PROTO_HEVEL_SERVER_H)
	$(CC) $(CFLAGS) -c hevel.c

$(SWC_DIR)/protocol/swc_snap-client-protocol.h: $(SWC_DIR)/protocol/swc_snap.xml
	wayland-scanner client-header < $(SWC_DIR)/protocol/swc_snap.xml > $(SWC_DIR)/protocol/swc_snap-client-protocol.h

$(SWC_DIR)/protocol/swc_snap-protocol.c: $(SWC_DIR)/protocol/swc_snap.xml
	wayland-scanner code < $(SWC_DIR)/protocol/swc_snap.xml > $(SWC_DIR)/protocol/swc_snap-protocol.c

$(SWC_DIR)/protocol/swc-client-protocol.h: $(SWC_DIR)/protocol/swc.xml
	wayland-scanner client-header < $(SWC_DIR)/protocol/swc.xml > $(SWC_DIR)/protocol/swc-client-protocol.h

$(SWC_DIR)/protocol/swc-client-protocol.c: $(SWC_DIR)/protocol/swc.xml
	wayland-scanner public-code < $(SWC_DIR)/protocol/swc.xml > $(SWC_DIR)/protocol/swc-client-protocol.c

swcsnap: swcsnap.o $(SWC_DIR)/protocol/swc_snap-protocol.o
	$(CC) $(LDFLAGS) -o swcsnap swcsnap.o $(SWC_DIR)/protocol/swc_snap-protocol.o $(SNAP_CLIENT_LDLIBS)

swcsnap.o: $(SNAP_C) $(SWC_DIR)/protocol/swc_snap-client-protocol.h
	$(CC) $(SNAP_CLIENT_CFLAGS) -c $(SNAP_C)

$(SWC_DIR)/protocol/swc_snap-protocol.o: $(SWC_DIR)/protocol/swc_snap-protocol.c
	$(CC) $(SNAP_CLIENT_CFLAGS) -c $(SWC_DIR)/protocol/swc_snap-protocol.c -o $(SWC_DIR)/protocol/swc_snap-protocol.o

$(PROTO_HEVEL_SERVER_O): $(PROTO_HEVEL_SERVER_C) $(PROTO_HEVEL_SERVER_H)
	$(CC) $(CFLAGS) -c $(PROTO_HEVEL_SERVER_C) -o $(PROTO_HEVEL_SERVER_O)

$(PROTO_HEVEL_SERVER_H) $(PROTO_HEVEL_CLIENT_H) $(PROTO_HEVEL_SERVER_C) $(PROTO_HEVEL_CLIENT_C): $(PROTO_HEVEL)
	wayland-scanner server-header $(PROTO_HEVEL) $(PROTO_HEVEL_SERVER_H)
	wayland-scanner client-header $(PROTO_HEVEL) $(PROTO_HEVEL_CLIENT_H)
	wayland-scanner private-code $(PROTO_HEVEL) $(PROTO_HEVEL_SERVER_C)
	wayland-scanner public-code $(PROTO_HEVEL) $(PROTO_HEVEL_CLIENT_C)

$(PROTO_HEVEL_CLIENT_O): $(PROTO_HEVEL_CLIENT_C) $(PROTO_HEVEL_CLIENT_H)
	$(CC) $(HBAR_CFLAGS) -c $(PROTO_HEVEL_CLIENT_C) -o $(PROTO_HEVEL_CLIENT_O)

hbar: $(HBAR_O) $(PROTO_HEVEL_CLIENT_O) $(SWC_DIR)/protocol/swc-client-protocol.o
	$(CC) $(LDFLAGS) -o hbar $(HBAR_O) $(PROTO_HEVEL_CLIENT_O) $(SWC_DIR)/protocol/swc-client-protocol.o $(HBAR_LDLIBS)

$(HBAR_O): $(PROTO_HEVEL_CLIENT_O) $(SWC_DIR)/protocol/swc-client-protocol.h
	$(CC) $(HBAR_CFLAGS) -c $(HBAR_C) -o $(HBAR_O)

$(SWC_DIR)/protocol/swc-client-protocol.o: $(SWC_DIR)/protocol/swc-client-protocol.c $(SWC_DIR)/protocol/swc-client-protocol.h
	$(CC) $(HBAR_CFLAGS) -c $(SWC_DIR)/protocol/swc-client-protocol.c -o $(SWC_DIR)/protocol/swc-client-protocol.o

FORCE:

$(SWC_LIB): FORCE
	$(MAKE) -C $(SWC_DIR)

clean:
	rm -f hevel hevel.o
	rm -f $(PROTO_HEVEL_SERVER_H) $(PROTO_HEVEL_CLIENT_H) $(PROTO_HEVEL_SERVER_C) $(PROTO_HEVEL_CLIENT_C) $(PROTO_HEVEL_SERVER_O) $(PROTO_HEVEL_CLIENT_O)
	rm -f $(SWC_DIR)/protocol/swc_snap-protocol.o $(SWC_DIR)/protocol/swc_snap-client-protocol.h
	rm -f $(SWC_DIR)/protocol/swc-client-protocol.o $(SWC_DIR)/protocol/swc-client-protocol.h $(SWC_DIR)/protocol/swc-client-protocol.c
	rm -f swcsnap swcsnap.o
	rm -f hbar extra/hbar/hbar.o
	$(MAKE) -C $(SWC_DIR) clean

install: hevel
	install -D -m 755 hevel $(DESTDIR)$(BINDIR)/hevel
	install -D -m 755 swcsnap $(DESTDIR)$(BINDIR)/swcsnap
	install -D -m 755 hbar $(DESTDIR)$(BINDIR)/hbar
	install -D -m 4755 swc-launch $(SWC_DIR)/launch/swc-launch

.PHONY: clean install FORCE
