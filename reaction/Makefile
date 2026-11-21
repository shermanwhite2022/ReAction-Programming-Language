# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
# ReAction! compiler (rc)
#
#   make              build ./rc   (needs gcc, yacc or bison, flex)
#   make DEBUG=0      build without the -DDEBUG progress messages
#   make check        compile every program in examples/ and tests/audit/ with rc and
#                     assemble it with asm6502 (no emulator needed)
#   make clean
#
# ASM is the assembler used by "make check".  By default it is the one built in ../asm6502
# (run "make" at the top of the repository first), else override: make check ASM=/path/asm6502
#
# The library (includes/) must stay next to rc: rc looks for it there, and the assembler
# source that rc writes includes "includes/*.inc".  Generated files go in build/.

CC     ?= gcc
YACC   ?= yacc
FLEX   ?= flex
DEBUG  ?= 1
CFLAGS ?= -w -g
ifeq ($(DEBUG),1)
CFLAGS += -DDEBUG
endif

SRC    = src
BUILD  = build
CFLAGS += -I$(SRC) -I$(BUILD)
ASM    ?= ../asm6502/asm6502

CSRCS  = main utils emit functions storage prune peephole
OBJS   = $(addprefix $(BUILD)/,$(addsuffix .o,$(CSRCS)) y.tab.o lex.yy.o)

rc: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) -lm

$(BUILD):
	mkdir -p $(BUILD)

# yacc and flex run inside build/ so their output lands there
$(BUILD)/y.tab.c $(BUILD)/y.tab.h: $(SRC)/reaction.y | $(BUILD)
	cd $(BUILD) && $(YACC) -d -t ../$(SRC)/reaction.y

$(BUILD)/lex.yy.c: $(SRC)/reaction.l $(BUILD)/y.tab.h | $(BUILD)
	cd $(BUILD) && $(FLEX) ../$(SRC)/reaction.l

$(BUILD)/%.o: $(SRC)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(BUILD)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/main.o $(BUILD)/utils.o $(BUILD)/emit.o $(BUILD)/functions.o $(BUILD)/storage.o: $(SRC)/utils.h $(SRC)/asmtypes.h
$(BUILD)/main.o $(BUILD)/emit.o $(BUILD)/functions.o: $(SRC)/emit.h $(SRC)/storage.h $(SRC)/functions.h
$(BUILD)/y.tab.o: $(SRC)/emit.h $(SRC)/functions.h
$(BUILD)/lex.yy.o: $(BUILD)/y.tab.h $(SRC)/utils.h

check: rc
	python3 tests/audit/run.py --rc ./rc --asm $(ASM)

clean:
	rm -rf $(BUILD) rc

.PHONY: check clean
