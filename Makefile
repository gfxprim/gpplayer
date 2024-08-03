include config.mk

CFLAGS?=-Wall -Wextra -O2 -ggdb
CFLAGS+=$(shell gfxprim-config --cflags)
LDLIBS=-lgfxprim $(shell gfxprim-config --libs-widgets --libs-loaders) -lasound
BIN=gpplayer
CSOURCES=$(wildcard *.c)
DEP=$(CSOURCES:.c=.dep)

all: $(DEP) $(BIN)

-include $(DEP)

%.dep: %.c
	$(CC) $(CFLAGS) -M $< -o $@

$(BIN): audio_mixer.o audio_output.o audio_decoder.o gpplayer_conf.o playlist.o

ifdef HAVE_MPG123_H
$(BIN): audio_decoder_mpg123.o
LDLIBS+=-lmpg123
endif

ifdef HAVE_MPV_CLIENT_H
$(BIN): audio_decoder_mpv.o
LDLIBS+=-lmpv
endif

install:
	install -D $(BIN) -t $(DESTDIR)/usr/bin/
	install -m 644 -D layout.json $(DESTDIR)/etc/gp_apps/$(BIN)/layout.json
	install -d $(DESTDIR)/usr/share/applications/
	install -m 644 $(BIN).desktop -t $(DESTDIR)/usr/share/applications/
	install -d $(DESTDIR)/usr/share/$(BIN)/
	install -m 644 $(BIN).png -t $(DESTDIR)/usr/share/$(BIN)/

clean:
	rm -f $(BIN) *.dep *.o
