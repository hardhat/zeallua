# Minimal ucsim regression harness for compiler-generated code.
HOST_COMPILER ?= ../zeallua_host
SIM ?= ucsim_z80
ASM ?= z80asm
OBJCOPY ?= sdobjcopy
TEST ?= program

STUB_ASM := ucsim_stub.asm
STUB_BIN := out/ucsim_stub.bin
STUB_IHX := out/ucsim_stub.ihx
PROGRAM_LUA := $(TEST).lua
PROGRAM_BIN := out/$(TEST).bin
PROGRAM_IHX := out/$(TEST).ihx
PROGRAM_SYM := out/$(TEST).sym
COMMANDS := out/$(TEST).commands.txt
ACTUAL := out/$(TEST).actual.txt
ifeq ($(TEST),program)
EXPECTED := expected.txt
else
EXPECTED := $(TEST).expected.txt
endif

.PHONY: all run check check-arith run-arith clean

all: check

check-arith:
	$(MAKE) check TEST=arith

run-arith:
	$(MAKE) run TEST=arith

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
	addr_hex=$$(awk '/^global_vars[[:space:]]+EQU[[:space:]]+0x/ { print $$3; exit }' $(PROGRAM_SYM)); \
	addr_no_prefix=$${addr_hex#0x}; \
	end_hex=$$(printf '0x%04X' $$((0x$$addr_no_prefix + 6))); \
	printf 'step 5000\ndump /h rom %s %s\nquit\n' "$$addr_hex" "$$end_hex" > $@

$(ACTUAL): $(STUB_IHX) $(PROGRAM_IHX) $(COMMANDS)
	addr_hex=$$(awk '/^global_vars[[:space:]]+EQU[[:space:]]+0x/ { print $$3; exit }' $(PROGRAM_SYM)); \
	addr_lc=$$(printf '%s' "$$addr_hex" | tr 'A-F' 'a-f'); \
	$(SIM) -q -X 10M -t Z80 $(STUB_IHX) $(PROGRAM_IHX) < $(COMMANDS) | grep "^$$addr_lc " | sed 's/^0x[0-9a-f]\{4\} //' > $@

clean:
	rm -rf out