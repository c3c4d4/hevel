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

hevel: $(SWC_LIB) hevel.o
	$(CC) $(LDFLAGS) -o hevel hevel.o $(LDLIBS)

hevel.o: hevel.c $(SWC_DIR)/libswc/swc.h
	$(CC) $(CFLAGS) -c hevel.c

$(SWC_LIB):
	$(MAKE) -C $(SWC_DIR)

clean:
	rm -f hevel hevel.o
	$(MAKE) -C $(SWC_DIR) clean

install: hevel
	install -D -m 755 hevel $(DESTDIR)$(BINDIR)/hevel

.PHONY: clean install
