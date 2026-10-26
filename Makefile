# SPDX-License-Identifier: GPL-2.0-or-later

PREFIX ?= /usr
BINDIR ?= $(PREFIX)/bin

CFLAGS ?= -O2 -g
CFLAGS += -std=gnu11 -Wall -Wextra -MMD -MP -D_GNU_SOURCE

CFLAGS += $(shell gfxprim-config --cflags)
LDLIBS += $(shell gfxprim-config --libs --libs-widgets)

BIN = wordtest
SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)

all: $(BIN)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN): $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

install: $(BIN)
	install -d $(DESTDIR)/etc/gp_apps/$(BIN)/ $(DESTDIR)$(BINDIR) $(DESTDIR)$(PREFIX)/share/$(BIN)/
	install -m 644 layout.json $(DESTDIR)/etc/gp_apps/$(BIN)/
	install -m 755 $(BIN) $(DESTDIR)$(BINDIR)
	install -d $(DESTDIR)$(PREFIX)/share/applications/
	install -m 644 $(BIN).desktop $(DESTDIR)$(PREFIX)/share/applications/

clean:
	rm -f $(BIN) $(OBJS) $(OBJS:.o=.d)

.PHONY: all install clean

-include $(wildcard *.d)
