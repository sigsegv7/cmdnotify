CFLAGS = -pedantic
CFILES = cmdnotify.c
CC = gcc
BIN_LOC = bin/cmdnotify

$(BIN_LOC): $(CFILES)
	mkdir -p $(@D)
	$(CC) $(CFLAGS) $(CFILES) -o $@

.PHONY: install
install:
	install $(BIN_LOC) /bin/
