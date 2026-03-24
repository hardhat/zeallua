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
STUB_LABELS := out/ucsim_stub.labels.txt
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

STDOUT_TESTS := print_stdout hello_world factorial_readme fibonacci_readme builtin_input builtin_input_eof builtin_readfile builtin_writefile builtin_readfile_missing builtin_readfile_empty builtin_writefile_missing builtin_writefile_number builtin_open_read_close builtin_open_write_close builtin_open_missing

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

ifeq ($(TEST),builtin_input)
DUMP_BYTES := 10
endif

ifeq ($(TEST),builtin_input_eof)
DUMP_BYTES := 9
endif

ifeq ($(TEST),builtin_readfile)
DUMP_BYTES := 5
endif

ifeq ($(TEST),builtin_writefile)
DUMP_BYTES := 9
endif

ifeq ($(TEST),builtin_readfile_missing)
DUMP_BYTES := 4
endif

ifeq ($(TEST),builtin_readfile_empty)
DUMP_BYTES := 1
endif

ifeq ($(TEST),builtin_writefile_missing)
DUMP_BYTES := 6
endif

ifeq ($(TEST),builtin_writefile_number)
DUMP_BYTES := 9
endif

ifeq ($(TEST),builtin_open_read_close)
DUMP_BYTES := 17
endif

ifeq ($(TEST),builtin_open_write_close)
DUMP_BYTES := 17
endif

ifeq ($(TEST),builtin_open_missing)
DUMP_BYTES := 4
endif

ifeq ($(TEST),table_reclaim_queue_smoke)
DUMP_SYMBOL := pending_table_reclaim_tail
DUMP_BYTES := 1
endif

ifeq ($(TEST),table_reclaim_queue_overflow)
DUMP_SYMBOL := pending_table_reclaim_dropcnt
DUMP_BYTES := 2
endif

ifeq ($(TEST),gc_trigger_counters)
DUMP_SYMBOL := gc_trigger_soft_count
DUMP_BYTES := 4
STEP_COUNT := 2000000
endif

ifeq ($(TEST),table_gc_hysteresis_latch)
DUMP_SYMBOL := gc_trigger_soft_count
DUMP_BYTES := 6
STEP_COUNT := 2500000
endif

ifeq ($(TEST),table_gc_hysteresis_clear)
DUMP_SYMBOL := gc_soft_hysteresis_latched
DUMP_BYTES := 2
STEP_COUNT := 200000
endif

ifeq ($(TEST),gc_string_counters)
DUMP_SYMBOL := gc_mark_string_count
DUMP_BYTES := 10
STEP_COUNT := 2500000
endif

ifeq ($(TEST),string_gc_pressure_counters)
DUMP_SYMBOL := gc_trigger_soft_count
DUMP_BYTES := 6
STEP_COUNT := 2500000
endif

ifeq ($(TEST),string_oom_counters)
DUMP_SYMBOL := alloc_fail_count
DUMP_BYTES := 6
STEP_COUNT := 2500000
endif

ifeq ($(TEST),string_freelist_corrupt_counter)
DUMP_SYMBOL := string_freelist_corrupt_count
DUMP_BYTES := 2
STEP_COUNT := 2500000
endif

ifeq ($(TEST),string_freelist_corrupt_counter_gate_off)
DUMP_SYMBOL := string_freelist_corrupt_count
DUMP_BYTES := 2
STEP_COUNT := 2500000
endif

ifeq ($(TEST),string_force_sweep_counters)
DUMP_SYMBOL := gc_trigger_force_count
DUMP_BYTES := 2
STEP_COUNT := 2500000
endif

ifeq ($(TEST),string_oom_dual_counters)
DUMP_SYMBOL := alloc_fail_count
DUMP_BYTES := 6
STEP_COUNT := 2500000
endif

ifeq ($(TEST),allocator_watermark_updates)
DUMP_SYMBOL := gc_low_watermark
DUMP_BYTES := 4
STEP_COUNT := 2500000
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
	local_loop_concat_dynamic \
	local_loop_tostring_dynamic \
	string_stress_mixed_lifetimes \
	string_gc_pressure_counters \
	string_oom_counters \
	string_freelist_corrupt_counter \
	string_freelist_corrupt_counter_gate_off \
	string_force_sweep_counters \
	string_oom_dual_counters \
	allocator_watermark_updates \
	pow_basic \
	pow_assoc \
	builtin_type \
	builtin_input \
	builtin_input_eof \
	builtin_readfile \
	builtin_writefile \
	builtin_readfile_missing \
	builtin_readfile_empty \
	builtin_writefile_missing \
	builtin_writefile_number \
	builtin_open_read_close \
	builtin_open_write_close \
	builtin_open_missing \
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
	table_mark_roots_counter \
	table_reclaim_queue_smoke \
	table_reclaim_queue_overflow \
	table_gc_hysteresis_latch \
	table_gc_hysteresis_clear \
	gc_trigger_counters \
	gc_string_counters \
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
SHARED_TEST_ARTIFACTS := $(STUB_IHX) $(STUB_LABELS)

DIAG_TESTS := \
	diag_unterminated_string \
	diag_unexpected_char \
	diag_tilde_without_eq \
	diag_missing_end \
	diag_expected_then \
	diag_expected_do \
	diag_expected_expr \
	diag_overflow_constants \
	diag_overflow_globals \
	diag_overflow_functions

# check-diag-<name> targets: strip the diag_ prefix, convert _ to - for target names
DIAG_SUITE_CHECK_TARGETS := $(addprefix check-diag-,$(subst _,-,$(patsubst diag_%,%,$(DIAG_TESTS))))

.PHONY: all run check suite clean FORCE

.SILENT: suite check run out $(STUB_BIN) $(STUB_IHX) $(PROGRAM_BIN) $(PROGRAM_SIM_BIN) $(PROGRAM_IHX) $(COMMANDS) $(ACTUAL) clean

all: suite

suite: $(SHARED_TEST_ARTIFACTS) $(SUITE_CHECK_TARGETS) $(DIAG_SUITE_CHECK_TARGETS)

# check-diag-% must appear before check-% so the longer prefix takes precedence
check-diag-%: FORCE
	@actual=$$($(HOST_COMPILER) diag_$(subst -,_,$*).lua 2>&1 || true); \
	expected=$$(tr -d '\r' < diag_$(subst -,_,$*).expected.txt); \
	if [ "$$actual" = "$$expected" ]; then \
		printf 'PASS %s\n' 'diag_$(subst -,_,$*)'; \
	else \
		printf 'FAIL %s\n' 'diag_$(subst -,_,$*)' >&2; \
		printf 'Expected: [%s]\n' "$$expected" >&2; \
		printf 'Actual:   [%s]\n' "$$actual" >&2; \
		exit 1; \
	fi

check-%: $(SHARED_TEST_ARTIFACTS) FORCE
	$(MAKE) check TEST=$(subst -,_,$*)

run-%: $(SHARED_TEST_ARTIFACTS) FORCE
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

$(STUB_LABELS): $(STUB_ASM) | out
	$(ASM) -L -o $(STUB_BIN) $< > /dev/null 2> $@

$(STUB_BIN): $(STUB_LABELS)
	@:

$(STUB_IHX): $(STUB_BIN)
	$(OBJCOPY) -I binary -O ihex $< $@

$(PROGRAM_BIN): $(PROGRAM_LUA) $(HOST_COMPILER) | out
	$(HOST_COMPILER) $< $@

$(PROGRAM_SYM): $(PROGRAM_BIN)
	@:

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

$(COMMANDS): $(PROGRAM_IHX) $(PROGRAM_SYM) $(STUB_LABELS) FORCE
	if [ -n "$(DUMP_ADDR)" ]; then \
		addr_hex=$(DUMP_ADDR); \
	else \
		addr_hex=$$(awk '/^$(DUMP_SYMBOL)[[:space:]]+EQU[[:space:]]+0x/ { print $$3; exit }' $(PROGRAM_SYM)); \
	fi; \
	stub_exit_hex=$$(awk '/^_sys_exit:[[:space:]]+equ[[:space:]]+\$$[0-9A-Fa-f]+/ { printf "0x%s", substr($$3, 2); exit }' $(STUB_LABELS)); \
	pre_commands=; \
	if [ "$(TEST)" = "string_freelist_corrupt_counter_gate_off" ]; then \
		debug_flag_hex=$$(awk '/^string_freelist_debug_enabled[[:space:]]+EQU[[:space:]]+0x/ { print $$3; exit }' $(PROGRAM_SYM)); \
		freelist_head_hex=$$(awk '/^free_string_small_list[[:space:]]+EQU[[:space:]]+0x/ { print $$3; exit }' $(PROGRAM_SYM)); \
		if [ -n "$$debug_flag_hex" ] && [ -n "$$freelist_head_hex" ]; then \
			freelist_head_no_prefix=$${freelist_head_hex#0x}; \
			freelist_head_hi_hex=$$(printf '0x%04X' $$((0x$$freelist_head_no_prefix + 1))); \
			pre_commands=$$(printf 'rom[%s]=0\nrom[%s]=1\nrom[%s]=0\n' "$$debug_flag_hex" "$$freelist_head_hex" "$$freelist_head_hi_hex"); \
		fi; \
	elif [ "$(TEST)" = "table_gc_hysteresis_clear" ]; then \
		soft_latch_hex=$$(awk '/^gc_soft_hysteresis_latched[[:space:]]+EQU[[:space:]]+0x/ { print $$3; exit }' $(PROGRAM_SYM)); \
		force_latch_hex=$$(awk '/^gc_force_hysteresis_latched[[:space:]]+EQU[[:space:]]+0x/ { print $$3; exit }' $(PROGRAM_SYM)); \
		heap_ptr_hex=$$(awk '/^heap_ptr[[:space:]]+EQU[[:space:]]+0x/ { print $$3; exit }' $(PROGRAM_SYM)); \
		heap_space_hex=$$(awk '/^heap_space[[:space:]]+EQU[[:space:]]+0x/ { print $$3; exit }' $(PROGRAM_SYM)); \
		if [ -n "$$soft_latch_hex" ] && [ -n "$$force_latch_hex" ] && [ -n "$$heap_ptr_hex" ] && [ -n "$$heap_space_hex" ]; then \
			heap_ptr_no_prefix=$${heap_ptr_hex#0x}; \
			heap_ptr_hi_hex=$$(printf '0x%04X' $$((0x$$heap_ptr_no_prefix + 1))); \
			heap_space_no_prefix=$${heap_space_hex#0x}; \
			heap_space_lo=$$((0x$$heap_space_no_prefix & 0xFF)); \
			heap_space_hi=$$(((0x$$heap_space_no_prefix >> 8) & 0xFF)); \
			pre_commands=$$(printf 'rom[%s]=1\nrom[%s]=1\nrom[%s]=%s\nrom[%s]=%s\n' "$$soft_latch_hex" "$$force_latch_hex" "$$heap_ptr_hex" "$$heap_space_lo" "$$heap_ptr_hi_hex" "$$heap_space_hi"); \
		fi; \
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
	if [ -n "$$stub_exit_hex" ]; then \
		printf '%s\ntbreak %s\nstep %s\ndump /h $(DUMP_SPACE) %s %s\nquit\n' "$$pre_commands" "$$stub_exit_hex" "$$step_count" "$$addr_hex" "$$end_hex" > $@; \
	else \
		printf '%s\nstep %s\ndump /h $(DUMP_SPACE) %s %s\nquit\n' "$$pre_commands" "$$step_count" "$$addr_hex" "$$end_hex" > $@; \
	fi

$(ACTUAL): $(STUB_IHX) $(PROGRAM_IHX) $(COMMANDS) FORCE
	if [ -n "$(DUMP_ADDR)" ]; then \
		addr_hex=$(DUMP_ADDR); \
	else \
		addr_hex=$$(awk '/^$(DUMP_SYMBOL)[[:space:]]+EQU[[:space:]]+0x/ { print $$3; exit }' $(PROGRAM_SYM)); \
	fi; \
	addr_no_prefix=$${addr_hex#0x}; \
	end_hex=$$(printf '0x%04X' $$((0x$$addr_no_prefix + $(DUMP_BYTES) - 1))); \
	$(SIM) -q -X 10M -t Z80 $(STUB_IHX) $(PROGRAM_IHX) < $(COMMANDS) | \
		awk -v start="$$addr_hex" -v end="$$end_hex" 'BEGIN { s = strtonum(start); e = strtonum(end); } /^0x[0-9a-fA-F]+[[:space:]]/ { a = strtonum($$1); if (a >= s && a <= e) print; }' | \
		sed 's/^0x[0-9a-fA-F]\{4\} //' > $@

clean:
	rm -rf out