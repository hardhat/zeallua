# Minimal ucsim regression harness for compiler-generated code.
HOST_COMPILER ?= ../zeallua_host
SIM ?= ucsim_z80
ASM ?= z80asm
OBJCOPY ?= sdobjcopy
TEST ?= program

MAKEFLAGS += --no-print-directory

STUB_ASM := ucsim_stub.asm
STUB_BIN := out/ucsim_stub.bin
STUB_IHX := out/ucsim_stub.ihx
PROGRAM_LUA := $(TEST).lua
PROGRAM_BIN := out/$(TEST).bin
PROGRAM_SIM_BIN := out/$(TEST).sim.bin
PROGRAM_IHX := out/$(TEST).ihx
PROGRAM_SYM := out/$(TEST).sym
COMMANDS := out/$(TEST).commands.txt
ACTUAL := out/$(TEST).actual.txt
ifeq ($(TEST),program)
EXPECTED := expected.txt
else
EXPECTED := $(TEST).expected.txt
endif

DUMP_SYMBOL ?= global_vars
DUMP_ADDR ?=
DUMP_BYTES ?= 7
DUMP_SPACE ?= rom
STEP_COUNT ?=

STDOUT_TESTS := print_stdout hello_world factorial_readme fibonacci_readme

ifneq ($(filter $(TEST),$(STDOUT_TESTS)),)
DUMP_ADDR := 0x0200

endif

ifeq ($(TEST),print_stdout)
DUMP_BYTES := 8
endif

ifeq ($(TEST),hello_world)
DUMP_BYTES := 12
endif

ifeq ($(TEST),factorial_readme)
DUMP_BYTES := 4
endif

ifeq ($(TEST),fibonacci_readme)
DUMP_BYTES := 26
STEP_COUNT := 2000000
endif

SUITE_TESTS := \
	program \
	arith \
	flow \
	flow_false \
	literal_true \
	literal_false \
	nil_branch \
	not_false \
	not_true \
	neg \
	div \
	mod \
	mul_basic \
	le \
	le_false \
	lt_negative \
	le_negative \
	gt_negative \
	ge_negative \
	lt_mixed_negative \
	le_mixed_negative_false \
	gt_mixed_negative_false \
	ge_mixed_negative \
	gt \
	gt_false \
	ge \
	ge_false \
	eq \
	eq_false \
	ne \
	ne_false \
	mixed_eq \
	mixed_eq_false \
	and_truthy \
	and_falsey \
	or_truthy \
	or_falsey \
	logic_precedence \
	function_add \
	function_nested_calls \
	closure_basic \
	closure_mutation \
	closure_nested_upvalue \
	local_function \
	print_stdout \
	hello_world \
	factorial_readme \
	fibonacci_readme \
	multiple_assignment \
	mixed_table_literal \
	string_key_content \
	len_string \
	len_table \
	concat_basic \
	concat_content \
	pow_basic \
	pow_assoc \
	builtin_type \
	builtin_tostring \
	builtin_tonumber \
	table_array_get \
	table_set_get \
	table_overwrite \
	table_overwrite_to_table \
	table_overwrite_to_number \
	table_missing_nil \
	table_nested_get \
	table_capacity \
	table_field_get \
	table_field_set \
	table_string_index_get \
	table_string_index_set \
	table_example_core \
	for_in_array \
	for_in_empty \
	else_taken \
	else_skipped \
	elseif_taken \
	elseif_fallback \
	else_local \
	elseif_local \
	branch_local_shadow_then \
	branch_local_shadow_else \
	nested_else_if \
	nested_else_if_false \
	nested_else_elseif \
	nested_else_elseif_fallback \
	elseif_chain_late \
	elseif_chain_fallback \
	nested_if \
	nested_if_false \
	while_count \
	while_zero \
	while_accumulate \
	while_local_shadow \
	repeat_until \
	repeat_local_shadow \
	for_count \
	for_step \
	for_negative_step \
	local_mixed_eq \
	local_mixed_eq_false \
	local_update \
	local_update_false \
	both_arith_eq \
	both_arith_eq_false

SUITE_CHECK_TARGETS := $(addprefix check-,$(subst _,-,$(SUITE_TESTS)))
SUITE_RUN_TARGETS := $(addprefix run-,$(subst _,-,$(SUITE_TESTS)))

.PHONY: all run check suite clean FORCE

.SILENT: suite check run out $(STUB_BIN) $(STUB_IHX) $(PROGRAM_BIN) $(PROGRAM_SIM_BIN) $(PROGRAM_IHX) $(COMMANDS) $(ACTUAL) clean

all: suite

suite: $(SUITE_CHECK_TARGETS)

check-%: FORCE
	$(MAKE) check TEST=$(subst -,_,$*)

run-%: FORCE
	$(MAKE) run TEST=$(subst -,_,$*)

FORCE:

check: $(ACTUAL)
	if tr -d '\r' < $(EXPECTED) | awk '{ print }' | cmp -s $(ACTUAL) -; then \
		printf 'PASS %s\n' '$(TEST)'; \
	else \
		printf 'FAIL %s\n' '$(TEST)' >&2; \
		exit 1; \
	fi

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

$(PROGRAM_SIM_BIN): $(PROGRAM_BIN) $(PROGRAM_SYM)
	bss_end_hex=$$(awk '/^bss_end[[:space:]]+EQU[[:space:]]+0x/ { print $$3; exit }' $(PROGRAM_SYM)); \
	if [ -n "$$bss_end_hex" ]; then \
		size_bytes=$$((0x$${bss_end_hex#0x} - 0x4000)); \
	else \
		size_bytes=$$(stat -c '%s' $<); \
	fi; \
	cp $< $@; \
	truncate -s $$size_bytes $@

$(PROGRAM_IHX): $(PROGRAM_SIM_BIN)
	$(OBJCOPY) -I binary -O ihex --change-addresses 0x4000 $< $@
	grep -vi '^:..00000[35]' $@ > $@.tmp
	mv $@.tmp $@

$(COMMANDS): $(PROGRAM_IHX) $(PROGRAM_SYM) FORCE
	if [ -n "$(DUMP_ADDR)" ]; then \
		addr_hex=$(DUMP_ADDR); \
	else \
		addr_hex=$$(awk '/^$(DUMP_SYMBOL)[[:space:]]+EQU[[:space:]]+0x/ { print $$3; exit }' $(PROGRAM_SYM)); \
	fi; \
	bss_start_hex=$$(awk '/^bss_start[[:space:]]+EQU[[:space:]]+0x/ { print $$3; exit }' $(PROGRAM_SYM)); \
	bss_end_hex=$$(awk '/^bss_end[[:space:]]+EQU[[:space:]]+0x/ { print $$3; exit }' $(PROGRAM_SYM)); \
	if [ -n "$(STEP_COUNT)" ]; then \
		step_count=$(STEP_COUNT); \
	elif [ -n "$$bss_start_hex" ] && [ -n "$$bss_end_hex" ]; then \
		step_count=$$((5000 + 10 * (0x$${bss_end_hex#0x} - 0x$${bss_start_hex#0x}))); \
	else \
		step_count=5000; \
	fi; \
	addr_no_prefix=$${addr_hex#0x}; \
	end_hex=$$(printf '0x%04X' $$((0x$$addr_no_prefix + $(DUMP_BYTES) - 1))); \
	printf 'step %s\ndump /h $(DUMP_SPACE) %s %s\nquit\n' "$$step_count" "$$addr_hex" "$$end_hex" > $@

$(ACTUAL): $(STUB_IHX) $(PROGRAM_IHX) $(COMMANDS) FORCE
	if [ -n "$(DUMP_ADDR)" ]; then \
		addr_hex=$(DUMP_ADDR); \
	else \
		addr_hex=$$(awk '/^$(DUMP_SYMBOL)[[:space:]]+EQU[[:space:]]+0x/ { print $$3; exit }' $(PROGRAM_SYM)); \
	fi; \
	$(SIM) -q -X 10M -t Z80 $(STUB_IHX) $(PROGRAM_IHX) < $(COMMANDS) | \
		grep '^0x[0-9a-f]\{4\} ' | sed 's/^0x[0-9a-f]\{4\} //' > $@

clean:
	rm -rf out