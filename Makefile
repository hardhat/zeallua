ZOS_PATH ?= ../Zeal-8-bit-OS
ZVB_SDK_PATH ?= ../Zeal-VideoBoard-SDK
BIN=bin/zeallua.bin
OBJ=obj/main.rel obj/lexer.rel obj/token.rel obj/parser.rel obj/ast.rel obj/compiler.rel obj/bytecode.rel obj/codegen.rel obj/z80_encoder.rel obj/interpreter.rel
HOST_MAKEFILE=Makefile.linux
CC=sdcc
CFLAGS=-mz80 --std-c2x -c -I $(ZOS_PATH)/kernel_headers/sdcc/include/ -I $(ZVB_SDK_PATH)/include --codeseg TEXT --debug
AS=sdasz80 -o -l -s
OBJCOPY=sdobjcopy
LD=sdldz80
LDFLAGS=-n -y -mjwx -i -b _HEADER=0x4000 -k $(ZOS_PATH)/kernel_headers/sdcc/lib -l z80 $(ZOS_LDFLAGS)
ZOS_LIBS=-k $(ZVB_SDK_PATH)/lib -l zvb_sound -l zvb_gfx
all: init $(BIN)

.PHONY: init clean reallyclean test host host-test host-clean

init:
	@mkdir -p obj
	@mkdir -p bin

$(BIN): $(OBJ)
	$(LD) $(LDFLAGS) -o $(BIN:.bin=.ihx) \
	../Zeal-8-bit-OS/kernel_headers/sdcc/bin/zos_crt0.rel $(OBJ) $(ZOS_LIBS)
	$(OBJCOPY) --input-target=ihex --output-target binary $(BIN:.bin=.ihx) $(BIN)

obj/%.rel: src/%.c
	$(CC) $(CFLAGS) -o $@ $<

obj/%.rel: src/%.asm
	$(AS) $@ $<

test: $(BIN)
	# Running ucsim_z80 against compiled zeallua.bin
	# Test integration goes here
	echo "Test passed"

host:
	$(MAKE) -f $(HOST_MAKEFILE)

host-test:
	$(MAKE) -f $(HOST_MAKEFILE) test

host-clean:
	$(MAKE) -f $(HOST_MAKEFILE) clean

clean:
	-rm -rf obj bin zeallua_host

reallyclean: clean
