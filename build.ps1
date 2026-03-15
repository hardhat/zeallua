$ZOS_PATH = "../Zeal-8-bit-OS"
$ZVB_SDK_PATH = "../Zeal-VideoBoard-SDK"

# Ensure output directories exist
New-Item -ItemType Directory -Force -Path obj
New-Item -ItemType Directory -Force -Path bin

# Source files
$sources = @(
    "src/main.c",
    "src/lexer.c",
    "src/token.c",
    "src/parser.c",
    "src/ast.c",
    "src/compiler.c",
    "src/bytecode.c",
    "src/codegen.c",
    "src/interpreter.c"
)

# Objects
$objects = @()

foreach ($src in $sources) {
    if (-not (Test-Path $src)) {
        continue
    }
    $obj = "obj/" + [System.IO.Path]::GetFileNameWithoutExtension($src) + ".rel"
    $objects += $obj
    
    echo "Compiling $src -> $obj"
    # Execute SDCC
    & sdcc -mz80 --std-c2x -c -I $ZOS_PATH/kernel_headers/sdcc/include/ -I $ZVB_SDK_PATH/include --codeseg TEXT --debug -o $obj $src
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Compilation of $src failed!"
        exit 1
    }
}

$objStr = $objects -join " "

echo "Linking into bin/zeallua.ihx"
& sdldz80 -n -y -mjwx -i -b _HEADER=0x4000 -k $ZOS_PATH/kernel_headers/sdcc/lib -l z80 $ZOS_LDFLAGS -o bin/zeallua.ihx $ZOS_PATH/kernel_headers/sdcc/bin/zos_crt0.rel $objects

if ($LASTEXITCODE -ne 0) {
    Write-Error "Linking failed!"
    exit 1
}

echo "Converting to zeallua.bin"
& sdobjcopy --input-target=ihex --output-target binary bin/zeallua.ihx bin/zeallua.bin

echo "Build complete! Output in bin/zeallua.bin"
