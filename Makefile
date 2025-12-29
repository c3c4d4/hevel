PREFIX = /usr/local
BINDIR = $(PREFIX)/bin

SWC_DIR = swc
SWC_LIB = $(SWC_DIR)/libswc/libswc.a

CFLAGS = -O2 -std=c11 -Wall -Wextra
CFLAGS += -I$(SWC_DIR)/libswc
CFLAGS += -I$(PREFIX)/include
CFLAGS += `pkg-config --cflags wayland-server libinput pixman-1 xkbcommon libdrm`

LDFLAGS = -L$(PREFIX)/lib -Wl,-rpath,$(PREFIX)/lib
LDLIBS = $(SWC_LIB) -lwld
LDLIBS += `pkg-config --libs wayland-server libinput pixman-1 xkbcommon libdrm libudev xcb xcb-composite xcb-ewmh xcb-icccm`

SNAP_CLIENT_CFLAGS = -O2 -std=c11 -Wall -Wextra
SNAP_CLIENT_CFLAGS += -I$(SWC_DIR)/protocol
SNAP_CLIENT_CFLAGS += `pkg-config --cflags wayland-client`

SNAP_CLIENT_LDLIBS = `pkg-config --libs wayland-client`

all: hevel swcsnap

hevel: $(SWC_LIB) hevel.o
	$(CC) $(LDFLAGS) -o hevel hevel.o $(LDLIBS)

hevel.o: hevel.c $(SWC_DIR)/libswc/swc.h
	$(CC) $(CFLAGS) -c hevel.c

$(SWC_DIR)/protocol/swc_snap-client-protocol.h: $(SWC_DIR)/protocol/swc_snap.xml
	wayland-scanner client-header < $(SWC_DIR)/protocol/swc_snap.xml > $(SWC_DIR)/protocol/swc_snap-client-protocol.h

$(SWC_DIR)/protocol/swc_snap-protocol.c: $(SWC_DIR)/protocol/swc_snap.xml
	wayland-scanner code < $(SWC_DIR)/protocol/swc_snap.xml > $(SWC_DIR)/protocol/swc_snap-protocol.c

swcsnap: swcsnap.o $(SWC_DIR)/protocol/swc_snap-protocol.o
	$(CC) $(LDFLAGS) -o swcsnap swcsnap.o $(SWC_DIR)/protocol/swc_snap-protocol.o $(SNAP_CLIENT_LDLIBS)


swcsnap.o: swcsnap.c $(SWC_DIR)/protocol/swc_snap-client-protocol.h
	$(CC) $(SNAP_CLIENT_CFLAGS) -c swcsnap.c

$(SWC_DIR)/protocol/swc_snap-protocol.o: $(SWC_DIR)/protocol/swc_snap-protocol.c
	$(CC) $(SNAP_CLIENT_CFLAGS) -c $(SWC_DIR)/protocol/swc_snap-protocol.c -o $(SWC_DIR)/protocol/swc_snap-protocol.o

FORCE:

$(SWC_LIB): FORCE
	$(MAKE) -C $(SWC_DIR)

clean:
	rm -f hevel hevel.o swcsnap swcsnap.o
	rm -f $(SWC_DIR)/protocol/swc_snap-protocol.o $(SWC_DIR)/protocol/swc_snap-client-protocol.h
	$(MAKE) -C $(SWC_DIR) clean

install: hevel swcsnap
	install -D -m 755 hevel $(DESTDIR)$(BINDIR)/hevel
	install -D -m 755 swcsnap $(DESTDIR)$(BINDIR)/swcsnap

.PHONY: clean install FORCE all
