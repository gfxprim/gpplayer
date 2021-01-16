CFLAGS=-W -Wall -Wextra -O2 $(shell gfxprim-config --cflags)
LDLIBS=-lgfxprim $(shell gfxprim-config --libs-widgets --libs-loaders) -lasound -lmpg123
BIN=gpplayer
DEP=$(BIN:=.dep)

all: $(DEP) $(BIN)

%.dep: %.c
	$(CC) $(CFLAGS) -M $< -o $@

$(BIN): audio_output.o playlist.o

install:
	install -D $(BIN) -t $(DESTDIR)/usr/bin/
	install -m 644 -D $(BIN).json $(DESTDIR)/etc/gp_apps/$(BIN)/layout.json

-include $(DEP)

clean:
	rm -f $(BIN) *.dep *.o
