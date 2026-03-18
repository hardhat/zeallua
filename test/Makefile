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

.PHONY: all run check check-arith run-arith check-flow run-flow check-flow-false run-flow-false check-literal-true run-literal-true check-literal-false run-literal-false check-le run-le check-le-false run-le-false check-gt run-gt check-gt-false run-gt-false check-ge run-ge check-ge-false run-ge-false check-eq run-eq check-eq-false run-eq-false check-ne run-ne check-ne-false run-ne-false suite clean

all: suite

suite:
	$(MAKE) check TEST=program
	$(MAKE) check TEST=arith
	$(MAKE) check TEST=flow
	$(MAKE) check TEST=flow_false
	$(MAKE) check TEST=literal_true
	$(MAKE) check TEST=literal_false
	$(MAKE) check TEST=le
	$(MAKE) check TEST=le_false
	$(MAKE) check TEST=gt
	$(MAKE) check TEST=gt_false
	$(MAKE) check TEST=ge
	$(MAKE) check TEST=ge_false
	$(MAKE) check TEST=eq
	$(MAKE) check TEST=eq_false
	$(MAKE) check TEST=ne
	$(MAKE) check TEST=ne_false

check-arith:
	$(MAKE) check TEST=arith

run-arith:
	$(MAKE) run TEST=arith

check-flow:
	$(MAKE) check TEST=flow

run-flow:
	$(MAKE) run TEST=flow

check-flow-false:
	$(MAKE) check TEST=flow_false

run-flow-false:
	$(MAKE) run TEST=flow_false

check-literal-true:
	$(MAKE) check TEST=literal_true

run-literal-true:
	$(MAKE) run TEST=literal_true

check-literal-false:
	$(MAKE) check TEST=literal_false

run-literal-false:
	$(MAKE) run TEST=literal_false

check-le:
	$(MAKE) check TEST=le

run-le:
	$(MAKE) run TEST=le

check-le-false:
	$(MAKE) check TEST=le_false

run-le-false:
	$(MAKE) run TEST=le_false

check-gt:
	$(MAKE) check TEST=gt

run-gt:
	$(MAKE) run TEST=gt

check-gt-false:
	$(MAKE) check TEST=gt_false

run-gt-false:
	$(MAKE) run TEST=gt_false

check-ge:
	$(MAKE) check TEST=ge

run-ge:
	$(MAKE) run TEST=ge

check-ge-false:
	$(MAKE) check TEST=ge_false

run-ge-false:
	$(MAKE) run TEST=ge_false

check-eq:
	$(MAKE) check TEST=eq

run-eq:
	$(MAKE) run TEST=eq

check-eq-false:
	$(MAKE) check TEST=eq_false

run-eq-false:
	$(MAKE) run TEST=eq_false

check-ne:
	$(MAKE) check TEST=ne

run-ne:
	$(MAKE) run TEST=ne

check-ne-false:
	$(MAKE) check TEST=ne_false

run-ne-false:
	$(MAKE) run TEST=ne_false

check: $(ACTUAL)
	tr -d '\r' < $(EXPECTED) | awk '{ print }' | cmp -s $(ACTUAL) -
	@echo "ucsim regression passed"

run: $(ACTUAL)
	@cat $(ACTUAL)

out:
	mkdir -p out

$(STUB_BIN): $(STUB_ASM) | out
	$(ASM) -o $@ $<

$(STUB_IHX): $(STUB_BIN)
	$(OBJCOPY) -I binary -O ihex $< $@

$(PROGRAM_BIN): $(PROGRAM_LUA) $(HOST_COMPILER) | out
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