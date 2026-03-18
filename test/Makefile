# Minimal ucsim regression harness for compiler-generated code.
HOST_COMPILER ?= ../zeallua_host
SIM ?= ucsim_z80
ASM ?= z80asm
OBJCOPY ?= sdobjcopy

STUB_ASM := ucsim_stub.asm
STUB_BIN := out/ucsim_stub.bin
STUB_IHX := out/ucsim_stub.ihx
PROGRAM_LUA := program.lua
PROGRAM_BIN := out/program.bin
PROGRAM_IHX := out/program.ihx
ACTUAL := out/actual.txt
EXPECTED := expected.txt

.PHONY: all run check clean

all: check

check: $(ACTUAL)
	tr -d '\r' < $(EXPECTED) | cmp -s $(ACTUAL) -
	@echo "ucsim regression passed"

run: $(ACTUAL)
	@cat $(ACTUAL)

out:
	mkdir -p out

$(STUB_BIN): $(STUB_ASM) | out
	$(ASM) -o $@ $<

$(STUB_IHX): $(STUB_BIN)
	$(OBJCOPY) -I binary -O ihex $< $@

$(PROGRAM_BIN): $(PROGRAM_LUA) | out
	$(HOST_COMPILER) $< $@

$(PROGRAM_IHX): $(PROGRAM_BIN)
	$(OBJCOPY) -I binary -O ihex --change-addresses 0x4000 $< $@
	grep -vi '^:..00000[35]' $@ > $@.tmp
	mv $@.tmp $@

$(ACTUAL): $(STUB_IHX) $(PROGRAM_IHX) commands.txt
	$(SIM) -q -X 10M -t Z80 $(STUB_IHX) $(PROGRAM_IHX) < commands.txt | grep '^0x0200' > $@

clean:
	rm -rf out