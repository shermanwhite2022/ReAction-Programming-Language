# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
# Top-level Makefile: builds both tools.
#
#   make              build asm6502/asm6502 and reaction/rc
#   make test         assembler tests, then compile-and-assemble checks for ReAction!
#   make install      PREFIX=/usr/local (override with PREFIX=... and DESTDIR=...)
#   make uninstall
#   make clean
#
# Needs: gcc, make, bison (3.0+), flex, yacc (bison -y is fine), python3 (tests only).

PREFIX ?= /usr/local
DESTDIR ?=

all: asm6502/asm6502 reaction/rc

asm6502/asm6502:
	$(MAKE) -C asm6502

reaction/rc:
	$(MAKE) -C reaction

test: all
	$(MAKE) -C asm6502 test
	$(MAKE) -C reaction check ASM=$(CURDIR)/asm6502/asm6502

# rc looks for its library in includes/ next to the rc binary, so the real binary and the
# library go in $(PREFIX)/lib/reaction and bin/rc is a small wrapper that runs it by full path.
# Programs are assembled with:  asm6502 -t atari800 prog.a
# (add -I $(PREFIX)/lib/reaction only if you compile with rc -k, which keeps the .include lines)
install: all
	install -d $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(PREFIX)/lib/reaction/includes
	install -m 755 asm6502/asm6502 $(DESTDIR)$(PREFIX)/bin/asm6502
	install -m 755 reaction/rc $(DESTDIR)$(PREFIX)/lib/reaction/rc
	install -m 644 reaction/includes/* $(DESTDIR)$(PREFIX)/lib/reaction/includes/
	printf '#!/bin/sh\nexec "%s/lib/reaction/rc" "$$@"\n' '$(PREFIX)' > $(DESTDIR)$(PREFIX)/bin/rc
	chmod 755 $(DESTDIR)$(PREFIX)/bin/rc

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/asm6502 $(DESTDIR)$(PREFIX)/bin/rc
	rm -rf $(DESTDIR)$(PREFIX)/lib/reaction

clean:
	$(MAKE) -C asm6502 clean
	$(MAKE) -C reaction clean

.PHONY: all test install uninstall clean
