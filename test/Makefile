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

.PHONY: all run check check-arith run-arith check-flow run-flow check-flow-false run-flow-false check-literal-true run-literal-true check-literal-false run-literal-false check-nil-branch run-nil-branch check-not-false run-not-false check-not-true run-not-true check-neg run-neg check-div run-div check-mod run-mod check-le run-le check-le-false run-le-false check-le-negative run-le-negative check-le-mixed-negative-false run-le-mixed-negative-false check-gt run-gt check-gt-false run-gt-false check-gt-negative run-gt-negative check-gt-mixed-negative-false run-gt-mixed-negative-false check-ge run-ge check-ge-false run-ge-false check-ge-negative run-ge-negative check-ge-mixed-negative run-ge-mixed-negative check-lt-negative run-lt-negative check-lt-mixed-negative run-lt-mixed-negative check-eq run-eq check-eq-false run-eq-false check-ne run-ne check-ne-false run-ne-false check-mixed-eq run-mixed-eq check-mixed-eq-false run-mixed-eq-false check-and-truthy run-and-truthy check-and-falsey run-and-falsey check-or-truthy run-or-truthy check-or-falsey run-or-falsey check-logic-precedence run-logic-precedence check-table-array-get run-table-array-get check-table-set-get run-table-set-get check-table-overwrite run-table-overwrite check-table-overwrite-to-table run-table-overwrite-to-table check-table-overwrite-to-number run-table-overwrite-to-number check-table-missing-nil run-table-missing-nil check-table-nested-get run-table-nested-get check-table-capacity run-table-capacity check-table-field-get run-table-field-get check-table-field-set run-table-field-set check-table-string-index-get run-table-string-index-get check-table-string-index-set run-table-string-index-set check-for-in-array run-for-in-array check-for-in-empty run-for-in-empty check-else-taken run-else-taken check-else-skipped run-else-skipped check-elseif-taken run-elseif-taken check-elseif-fallback run-elseif-fallback check-else-local run-else-local check-elseif-local run-elseif-local check-branch-local-shadow-then run-branch-local-shadow-then check-branch-local-shadow-else run-branch-local-shadow-else check-nested-else-if run-nested-else-if check-nested-else-if-false run-nested-else-if-false check-nested-else-elseif run-nested-else-elseif check-nested-else-elseif-fallback run-nested-else-elseif-fallback check-elseif-chain-late run-elseif-chain-late check-elseif-chain-fallback run-elseif-chain-fallback check-nested-if run-nested-if check-nested-if-false run-nested-if-false check-while-count run-while-count check-while-zero run-while-zero check-while-accumulate run-while-accumulate check-while-local-shadow run-while-local-shadow check-repeat-until run-repeat-until check-repeat-local-shadow run-repeat-local-shadow check-for-count run-for-count check-for-step run-for-step check-for-negative-step run-for-negative-step check-local-mixed-eq run-local-mixed-eq check-local-mixed-eq-false run-local-mixed-eq-false check-local-update run-local-update check-local-update-false run-local-update-false check-both-arith-eq run-both-arith-eq check-both-arith-eq-false run-both-arith-eq-false suite clean

all: suite

suite:
	$(MAKE) check TEST=program
	$(MAKE) check TEST=arith
	$(MAKE) check TEST=flow
	$(MAKE) check TEST=flow_false
	$(MAKE) check TEST=literal_true
	$(MAKE) check TEST=literal_false
	$(MAKE) check TEST=nil_branch
	$(MAKE) check TEST=not_false
	$(MAKE) check TEST=not_true
	$(MAKE) check TEST=neg
	$(MAKE) check TEST=div
	$(MAKE) check TEST=mod
	$(MAKE) check TEST=le
	$(MAKE) check TEST=le_false
	$(MAKE) check TEST=lt_negative
	$(MAKE) check TEST=le_negative
	$(MAKE) check TEST=gt_negative
	$(MAKE) check TEST=ge_negative
	$(MAKE) check TEST=lt_mixed_negative
	$(MAKE) check TEST=le_mixed_negative_false
	$(MAKE) check TEST=gt_mixed_negative_false
	$(MAKE) check TEST=ge_mixed_negative
	$(MAKE) check TEST=gt
	$(MAKE) check TEST=gt_false
	$(MAKE) check TEST=ge
	$(MAKE) check TEST=ge_false
	$(MAKE) check TEST=eq
	$(MAKE) check TEST=eq_false
	$(MAKE) check TEST=ne
	$(MAKE) check TEST=ne_false
	$(MAKE) check TEST=mixed_eq
	$(MAKE) check TEST=mixed_eq_false
	$(MAKE) check TEST=and_truthy
	$(MAKE) check TEST=and_falsey
	$(MAKE) check TEST=or_truthy
	$(MAKE) check TEST=or_falsey
	$(MAKE) check TEST=logic_precedence
	$(MAKE) check TEST=table_array_get
	$(MAKE) check TEST=table_set_get
	$(MAKE) check TEST=table_overwrite
	$(MAKE) check TEST=table_overwrite_to_table
	$(MAKE) check TEST=table_overwrite_to_number
	$(MAKE) check TEST=table_missing_nil
	$(MAKE) check TEST=table_nested_get
	$(MAKE) check TEST=table_capacity
	$(MAKE) check TEST=table_field_get
	$(MAKE) check TEST=table_field_set
	$(MAKE) check TEST=table_string_index_get
	$(MAKE) check TEST=table_string_index_set
	$(MAKE) check TEST=for_in_array
	$(MAKE) check TEST=for_in_empty
	$(MAKE) check TEST=else_taken
	$(MAKE) check TEST=else_skipped
	$(MAKE) check TEST=elseif_taken
	$(MAKE) check TEST=elseif_fallback
	$(MAKE) check TEST=else_local
	$(MAKE) check TEST=elseif_local
	$(MAKE) check TEST=branch_local_shadow_then
	$(MAKE) check TEST=branch_local_shadow_else
	$(MAKE) check TEST=nested_else_if
	$(MAKE) check TEST=nested_else_if_false
	$(MAKE) check TEST=nested_else_elseif
	$(MAKE) check TEST=nested_else_elseif_fallback
	$(MAKE) check TEST=elseif_chain_late
	$(MAKE) check TEST=elseif_chain_fallback
	$(MAKE) check TEST=nested_if
	$(MAKE) check TEST=nested_if_false
	$(MAKE) check TEST=while_count
	$(MAKE) check TEST=while_zero
	$(MAKE) check TEST=while_accumulate
	$(MAKE) check TEST=while_local_shadow
	$(MAKE) check TEST=repeat_until
	$(MAKE) check TEST=repeat_local_shadow
	$(MAKE) check TEST=for_count
	$(MAKE) check TEST=for_step
	$(MAKE) check TEST=for_negative_step
	$(MAKE) check TEST=local_mixed_eq
	$(MAKE) check TEST=local_mixed_eq_false
	$(MAKE) check TEST=local_update
	$(MAKE) check TEST=local_update_false
	$(MAKE) check TEST=both_arith_eq
	$(MAKE) check TEST=both_arith_eq_false

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

check-nil-branch:
	$(MAKE) check TEST=nil_branch

run-nil-branch:
	$(MAKE) run TEST=nil_branch

check-not-false:
	$(MAKE) check TEST=not_false

run-not-false:
	$(MAKE) run TEST=not_false

check-not-true:
	$(MAKE) check TEST=not_true

run-not-true:
	$(MAKE) run TEST=not_true

check-neg:
	$(MAKE) check TEST=neg

run-neg:
	$(MAKE) run TEST=neg

check-div:
	$(MAKE) check TEST=div

run-div:
	$(MAKE) run TEST=div

check-mod:
	$(MAKE) check TEST=mod

run-mod:
	$(MAKE) run TEST=mod

check-le:
	$(MAKE) check TEST=le

run-le:
	$(MAKE) run TEST=le

check-le-false:
	$(MAKE) check TEST=le_false

run-le-false:
	$(MAKE) run TEST=le_false

check-le-negative:
	$(MAKE) check TEST=le_negative

run-le-negative:
	$(MAKE) run TEST=le_negative

check-le-mixed-negative-false:
	$(MAKE) check TEST=le_mixed_negative_false

run-le-mixed-negative-false:
	$(MAKE) run TEST=le_mixed_negative_false

check-lt-negative:
	$(MAKE) check TEST=lt_negative

run-lt-negative:
	$(MAKE) run TEST=lt_negative

check-lt-mixed-negative:
	$(MAKE) check TEST=lt_mixed_negative

run-lt-mixed-negative:
	$(MAKE) run TEST=lt_mixed_negative

check-gt:
	$(MAKE) check TEST=gt

run-gt:
	$(MAKE) run TEST=gt

check-gt-false:
	$(MAKE) check TEST=gt_false

run-gt-false:
	$(MAKE) run TEST=gt_false

check-gt-negative:
	$(MAKE) check TEST=gt_negative

run-gt-negative:
	$(MAKE) run TEST=gt_negative

check-gt-mixed-negative-false:
	$(MAKE) check TEST=gt_mixed_negative_false

run-gt-mixed-negative-false:
	$(MAKE) run TEST=gt_mixed_negative_false

check-ge:
	$(MAKE) check TEST=ge

run-ge:
	$(MAKE) run TEST=ge

check-ge-false:
	$(MAKE) check TEST=ge_false

run-ge-false:
	$(MAKE) run TEST=ge_false

check-ge-negative:
	$(MAKE) check TEST=ge_negative

run-ge-negative:
	$(MAKE) run TEST=ge_negative

check-ge-mixed-negative:
	$(MAKE) check TEST=ge_mixed_negative

run-ge-mixed-negative:
	$(MAKE) run TEST=ge_mixed_negative

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

check-mixed-eq:
	$(MAKE) check TEST=mixed_eq

run-mixed-eq:
	$(MAKE) run TEST=mixed_eq

check-mixed-eq-false:
	$(MAKE) check TEST=mixed_eq_false

run-mixed-eq-false:
	$(MAKE) run TEST=mixed_eq_false

check-and-truthy:
	$(MAKE) check TEST=and_truthy

run-and-truthy:
	$(MAKE) run TEST=and_truthy

check-and-falsey:
	$(MAKE) check TEST=and_falsey

run-and-falsey:
	$(MAKE) run TEST=and_falsey

check-or-truthy:
	$(MAKE) check TEST=or_truthy

run-or-truthy:
	$(MAKE) run TEST=or_truthy

check-or-falsey:
	$(MAKE) check TEST=or_falsey

run-or-falsey:
	$(MAKE) run TEST=or_falsey

check-logic-precedence:
	$(MAKE) check TEST=logic_precedence

run-logic-precedence:
	$(MAKE) run TEST=logic_precedence

check-table-array-get:
	$(MAKE) check TEST=table_array_get

run-table-array-get:
	$(MAKE) run TEST=table_array_get

check-table-set-get:
	$(MAKE) check TEST=table_set_get

run-table-set-get:
	$(MAKE) run TEST=table_set_get

check-table-overwrite:
	$(MAKE) check TEST=table_overwrite

run-table-overwrite:
	$(MAKE) run TEST=table_overwrite

check-table-overwrite-to-table:
	$(MAKE) check TEST=table_overwrite_to_table

run-table-overwrite-to-table:
	$(MAKE) run TEST=table_overwrite_to_table

check-table-overwrite-to-number:
	$(MAKE) check TEST=table_overwrite_to_number

run-table-overwrite-to-number:
	$(MAKE) run TEST=table_overwrite_to_number

check-table-missing-nil:
	$(MAKE) check TEST=table_missing_nil

run-table-missing-nil:
	$(MAKE) run TEST=table_missing_nil

check-table-nested-get:
	$(MAKE) check TEST=table_nested_get

run-table-nested-get:
	$(MAKE) run TEST=table_nested_get

check-table-capacity:
	$(MAKE) check TEST=table_capacity

run-table-capacity:
	$(MAKE) run TEST=table_capacity

check-table-field-get:
	$(MAKE) check TEST=table_field_get

run-table-field-get:
	$(MAKE) run TEST=table_field_get

check-table-field-set:
	$(MAKE) check TEST=table_field_set

run-table-field-set:
	$(MAKE) run TEST=table_field_set

check-table-string-index-get:
	$(MAKE) check TEST=table_string_index_get

run-table-string-index-get:
	$(MAKE) run TEST=table_string_index_get

check-table-string-index-set:
	$(MAKE) check TEST=table_string_index_set

run-table-string-index-set:
	$(MAKE) run TEST=table_string_index_set

check-for-in-array:
	$(MAKE) check TEST=for_in_array

run-for-in-array:
	$(MAKE) run TEST=for_in_array

check-for-in-empty:
	$(MAKE) check TEST=for_in_empty

run-for-in-empty:
	$(MAKE) run TEST=for_in_empty

check-else-taken:
	$(MAKE) check TEST=else_taken

run-else-taken:
	$(MAKE) run TEST=else_taken

check-else-skipped:
	$(MAKE) check TEST=else_skipped

run-else-skipped:
	$(MAKE) run TEST=else_skipped

check-elseif-taken:
	$(MAKE) check TEST=elseif_taken

run-elseif-taken:
	$(MAKE) run TEST=elseif_taken

check-elseif-fallback:
	$(MAKE) check TEST=elseif_fallback

run-elseif-fallback:
	$(MAKE) run TEST=elseif_fallback

check-else-local:
	$(MAKE) check TEST=else_local

run-else-local:
	$(MAKE) run TEST=else_local

check-elseif-local:
	$(MAKE) check TEST=elseif_local

run-elseif-local:
	$(MAKE) run TEST=elseif_local

check-branch-local-shadow-then:
	$(MAKE) check TEST=branch_local_shadow_then

run-branch-local-shadow-then:
	$(MAKE) run TEST=branch_local_shadow_then

check-branch-local-shadow-else:
	$(MAKE) check TEST=branch_local_shadow_else

run-branch-local-shadow-else:
	$(MAKE) run TEST=branch_local_shadow_else

check-nested-else-if:
	$(MAKE) check TEST=nested_else_if

run-nested-else-if:
	$(MAKE) run TEST=nested_else_if

check-nested-else-if-false:
	$(MAKE) check TEST=nested_else_if_false

run-nested-else-if-false:
	$(MAKE) run TEST=nested_else_if_false

check-nested-else-elseif:
	$(MAKE) check TEST=nested_else_elseif

run-nested-else-elseif:
	$(MAKE) run TEST=nested_else_elseif

check-nested-else-elseif-fallback:
	$(MAKE) check TEST=nested_else_elseif_fallback

run-nested-else-elseif-fallback:
	$(MAKE) run TEST=nested_else_elseif_fallback

check-elseif-chain-late:
	$(MAKE) check TEST=elseif_chain_late

run-elseif-chain-late:
	$(MAKE) run TEST=elseif_chain_late

check-elseif-chain-fallback:
	$(MAKE) check TEST=elseif_chain_fallback

run-elseif-chain-fallback:
	$(MAKE) run TEST=elseif_chain_fallback

check-nested-if:
	$(MAKE) check TEST=nested_if

run-nested-if:
	$(MAKE) run TEST=nested_if

check-nested-if-false:
	$(MAKE) check TEST=nested_if_false

run-nested-if-false:
	$(MAKE) run TEST=nested_if_false

check-while-count:
	$(MAKE) check TEST=while_count

run-while-count:
	$(MAKE) run TEST=while_count

check-while-zero:
	$(MAKE) check TEST=while_zero

run-while-zero:
	$(MAKE) run TEST=while_zero

check-while-accumulate:
	$(MAKE) check TEST=while_accumulate

run-while-accumulate:
	$(MAKE) run TEST=while_accumulate

check-while-local-shadow:
	$(MAKE) check TEST=while_local_shadow

run-while-local-shadow:
	$(MAKE) run TEST=while_local_shadow

check-repeat-until:
	$(MAKE) check TEST=repeat_until

run-repeat-until:
	$(MAKE) run TEST=repeat_until

check-repeat-local-shadow:
	$(MAKE) check TEST=repeat_local_shadow

run-repeat-local-shadow:
	$(MAKE) run TEST=repeat_local_shadow

check-for-count:
	$(MAKE) check TEST=for_count

run-for-count:
	$(MAKE) run TEST=for_count

check-for-step:
	$(MAKE) check TEST=for_step

run-for-step:
	$(MAKE) run TEST=for_step

check-for-negative-step:
	$(MAKE) check TEST=for_negative_step

run-for-negative-step:
	$(MAKE) run TEST=for_negative_step

check-local-mixed-eq:
	$(MAKE) check TEST=local_mixed_eq

run-local-mixed-eq:
	$(MAKE) run TEST=local_mixed_eq

check-local-mixed-eq-false:
	$(MAKE) check TEST=local_mixed_eq_false

run-local-mixed-eq-false:
	$(MAKE) run TEST=local_mixed_eq_false

check-local-update:
	$(MAKE) check TEST=local_update

run-local-update:
	$(MAKE) run TEST=local_update

check-local-update-false:
	$(MAKE) check TEST=local_update_false

run-local-update-false:
	$(MAKE) run TEST=local_update_false

check-both-arith-eq:
	$(MAKE) check TEST=both_arith_eq

run-both-arith-eq:
	$(MAKE) run TEST=both_arith_eq

check-both-arith-eq-false:
	$(MAKE) check TEST=both_arith_eq_false

run-both-arith-eq-false:
	$(MAKE) run TEST=both_arith_eq_false

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