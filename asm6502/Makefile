# Author: Sherman White Jr., email: subscribeandnotifyme@gmail.com c. 2026
# asm6502 - 6502 macro assembler
#
#   make          build ./asm6502   (needs gcc, bison 3.0+, flex)
#   make test     run tests/run_tests.py
#   make clean
#
# Generated parser/lexer files and object files go in build/ (not tracked by git).

CC      ?= gcc
CFLAGS  ?= -Wall -g
BISON   ?= bison
FLEX    ?= flex

SRC     = src
BUILD   = build
CFLAGS += -I$(SRC) -I$(BUILD)

CSRCS   = main asm symtab output image list utils
OBJS    = $(addprefix $(BUILD)/,$(addsuffix .o,$(CSRCS)) 6502parse.tab.o lex.yy.o)

asm6502: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(BUILD):
	mkdir -p $(BUILD)

# bison and flex run inside build/ so their output lands there
$(BUILD)/6502parse.tab.c $(BUILD)/6502parse.tab.h: $(SRC)/6502parse.y | $(BUILD)
	cd $(BUILD) && $(BISON) -d -v ../$(SRC)/6502parse.y

$(BUILD)/lex.yy.c: $(SRC)/6502parse.l $(BUILD)/6502parse.tab.h | $(BUILD)
	cd $(BUILD) && $(FLEX) -o lex.yy.c ../$(SRC)/6502parse.l

$(BUILD)/%.o: $(SRC)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/%.o: $(BUILD)/%.c | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD)/main.o $(BUILD)/asm.o $(BUILD)/symtab.o $(BUILD)/output.o $(BUILD)/image.o \
$(BUILD)/list.o $(BUILD)/6502parse.tab.o $(BUILD)/lex.yy.o: $(SRC)/asm.h $(SRC)/asmtypes.h
$(BUILD)/utils.o $(BUILD)/lex.yy.o: $(SRC)/utils.h
$(BUILD)/lex.yy.o: $(BUILD)/6502parse.tab.h

test: asm6502
	python3 tests/run_tests.py

clean:
	rm -rf $(BUILD) asm6502

.PHONY: test clean
