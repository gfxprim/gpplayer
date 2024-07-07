include config.mk
-include $(DEP)

CFLAGS?=-Wall -Wextra -O2 -ggdb
CFLAGS+=$(shell gfxprim-config --cflags)
LDLIBS=-lgfxprim $(shell gfxprim-config --libs-widgets --libs-loaders) -lasound
BIN=gpplayer
DEP=$(BIN:=.dep)

all: $(DEP) $(BIN)

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


clean:
	rm -f $(BIN) *.dep *.o
