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
COMMANDS := out/commands.txt
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

$(COMMANDS): $(PROGRAM_IHX)
	addr_hex=$$(awk '/^global_vars[[:space:]]+EQU[[:space:]]+0x/ { print $$3; exit }' out/program.sym); \
	addr_no_prefix=$${addr_hex#0x}; \
	end_hex=$$(printf '0x%04X' $$((0x$$addr_no_prefix + 6))); \
	printf 'step 5000\ndump /h rom %s %s\nquit\n' "$$addr_hex" "$$end_hex" > $@

$(ACTUAL): $(STUB_IHX) $(PROGRAM_IHX) $(COMMANDS)
	addr_hex=$$(awk '/^global_vars[[:space:]]+EQU[[:space:]]+0x/ { print $$3; exit }' out/program.sym); \
	addr_lc=$$(printf '%s' "$$addr_hex" | tr 'A-F' 'a-f'); \
	$(SIM) -q -X 10M -t Z80 $(STUB_IHX) $(PROGRAM_IHX) < $(COMMANDS) | grep "^$$addr_lc " | sed 's/^0x[0-9a-f]\{4\} //' > $@

clean:
	rm -rf out