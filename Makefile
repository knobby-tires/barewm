~/CLionProjects/untitled main*
❯ PREFIX ?= /usr/local
  DESTDIR ?=
  BINDIR ?= $(PREFIX)/bin

  CC ?= gcc
  CFLAGS ?= -std=c99 -Wall -Wextra -pedantic -O2

  sowm: sowm.c
  $(CC) -o $@ $< $(CFLAGS) $(LDFLAGS)

  clean:
  rm -f barewm

  install:
  mkdir -p $(DESTDIR)$(BINDIR)
  cp -f barewm $(DESTDIR)$(BINDIR)
  chmod 755 $(DESTDIR)$(BINDIR)/barewm

  uninstall:
  rm -f $(DESTDIR)$(BINDIR)/barewm

  .PHONY: clean install uninstall
