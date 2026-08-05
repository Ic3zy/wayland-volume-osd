CC ?= gcc
CFLAGS ?= -std=c17 -Wall -Wextra -pedantic -Werror=implicit-function-declaration -fPIC -g
INCLUDES = -Iinclude -Isrc

WAYLAND_CFLAGS := $(shell pkg-config --cflags wayland-client cairo 2>/dev/null)
WAYLAND_LIBS := $(shell pkg-config --libs wayland-client cairo 2>/dev/null || echo "-lwayland-client -lcairo -lm")

WAYLAND_SCANNER := $(shell pkg-config --variable=wayland_scanner wayland-scanner 2>/dev/null || echo "wayland-scanner")

PROTO_HEADERS = src/xdg-shell-client-protocol.h src/wlr-layer-shell-unstable-v1-client-protocol.h
PROTO_SRCS = src/xdg-shell-protocol.c src/wlr-layer-shell-unstable-v1-protocol.c

LIB_SRCS = src/wayland.c src/layer_shell.c src/xdg_backend.c src/render.c src/animation.c src/osd.c $(PROTO_SRCS)
LIB_OBJS = $(LIB_SRCS:.c=.o)

all: libosd.so libosd.a demo test_phase1

src/xdg-shell-client-protocol.h: protocols/xdg-shell.xml
	$(WAYLAND_SCANNER) client-header $< $@

src/xdg-shell-protocol.c: protocols/xdg-shell.xml
	$(WAYLAND_SCANNER) private-code $< $@

src/wlr-layer-shell-unstable-v1-client-protocol.h: protocols/wlr-layer-shell-unstable-v1.xml
	$(WAYLAND_SCANNER) client-header $< $@

src/wlr-layer-shell-unstable-v1-protocol.c: protocols/wlr-layer-shell-unstable-v1.xml
	$(WAYLAND_SCANNER) private-code $< $@

%.o: %.c $(PROTO_HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) $(WAYLAND_CFLAGS) -c $< -o $@

libosd.so: $(LIB_OBJS)
	$(CC) -shared $(LIB_OBJS) -o $@ $(WAYLAND_LIBS)

libosd.a: $(LIB_OBJS)
	ar rcs $@ $(LIB_OBJS)

demo: demo.c libosd.a $(PROTO_HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) $(WAYLAND_CFLAGS) demo.c libosd.a -o $@ $(WAYLAND_LIBS)

test_phase1: test_phase1.c $(LIB_OBJS) $(PROTO_HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) $(WAYLAND_CFLAGS) test_phase1.c $(LIB_OBJS) -o $@ $(WAYLAND_LIBS)

clean:
	rm -f src/*.o src/*-protocol.c src/*-protocol.h libosd.so libosd.a demo test_phase1

.PHONY: all clean
