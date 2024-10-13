CFLAGS?=-Wall -Wextra -O2 -ggdb
BIN=gpplayer
$(BIN): LDLIBS=-lgfxprim $(shell gfxprim-config --libs-widgets --libs-loaders) -lasound
CSOURCES=$(filter-out audio_decoder_mpg123.c audio_decoder_mpv.c, $(wildcard *.c))
DEP=$(CSOURCES:.c=.dep)
OBJ=$(CSOURCES:.c=.o)

all: $(BIN) $(DEP)

%.dep: %.c
	$(CC) $(CFLAGS) -M $< -o $@

-include config.mk

ifdef HAVE_MPG123_H
DEP+=audio_decoder_mpg123.dep
OBJ+=audio_decoder_mpg123.o
$(BIN): LDLIBS+=-lmpg123
endif

ifdef HAVE_MPV_CLIENT_H
DEP+=audio_decoder_mpv.dep
OBJ+=audio_decoder_mpv.o
$(BIN): LDLIBS+=-lmpv
endif

$(DEP) $(OBJ): CFLAGS+=$(shell gfxprim-config --cflags)

$(BIN): $(OBJ)

ifeq ($(MAKECMDGOALS),all)
-include $(DEP)
endif

ifeq ($(MAKECMDGOALS),)
-include $(DEP)
endif

install:
	install -D $(BIN) -t $(DESTDIR)/usr/bin/
	install -m 644 -D layout.json $(DESTDIR)/etc/gp_apps/$(BIN)/layout.json
	install -d $(DESTDIR)/usr/share/applications/
	install -m 644 $(BIN).desktop -t $(DESTDIR)/usr/share/applications/
	install -d $(DESTDIR)/usr/share/$(BIN)/
	install -m 644 $(BIN).png -t $(DESTDIR)/usr/share/$(BIN)/

distclean: clean
	rm -f config.h config.mk

clean:
	rm -f $(BIN) *.dep *.o
