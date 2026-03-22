#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#ifdef __SDCC
#include "zos_sys.h"
#include "zos_vfs.h"
#include "zos_video.h"
#include "zos_time.h"
#include "zos_keyboard.h"
#else
#include "zos_host_stub.h"
#endif
#include "lexer.h"
#include "parser.h"
#include "compiler.h"
#include "codegen.h"

static CompiledChunk compiled;

// Zeal OS specific print function
void z_print(const char* str, uint16_t len) {
    uint16_t out_len = len;
    write(DEV_STDOUT, str, &out_len);
}

// Zeal OS string length helper
uint16_t z_strlen(const char* str) {
    uint16_t len = 0;
    while(str[len] != '\0') len++;
    return len;
}

static void z_print_cstr(const char* str) {
    z_print(str, z_strlen(str));
}

static void z_print_u16(uint16_t value) {
    char buf[6];
    uint8_t i = 0;
    uint8_t j;

    if (value == 0) {
        z_print("0", 1);
        return;
    }

    while (value > 0 && i < sizeof(buf)) {
        buf[i++] = (char)('0' + (value % 10));
        value /= 10;
    }

    for (j = 0; j < i / 2; j++) {
        char t = buf[j];
        buf[j] = buf[i - j - 1];
        buf[i - j - 1] = t;
    }
    z_print(buf, i);
}

static void z_print_error(const char* file_name, uint16_t line, uint16_t column, const char* msg) {
    if (file_name && file_name[0]) {
        z_print_cstr(file_name);
    } else {
        z_print("<input>", 7);
    }
    z_print(":", 1);
    z_print_u16(line);
    z_print(":", 1);
    z_print_u16(column);
    z_print(": error: ", 9);
    if (msg && msg[0]) {
        z_print_cstr(msg);
    } else {
        z_print("Unknown error", 13);
    }
    z_print("\n", 1);
}

static char file_buffer[16384];

int main(int argc, const char* argv[]) {
    if (argc < 2) {
        const char usage[] = "Usage: zeallua <file.lua>\n";
        z_print(usage, sizeof(usage) - 1);
        exit(1);
    }
    
    zos_dev_t file_dev = open(argv[1], O_RDONLY);
    if (file_dev < 0) {
        const char err[] = "Error opening file\n";
        z_print(err, sizeof(err) - 1);
        exit(1);
    }

    uint16_t file_len = 0;
    while (file_len < sizeof(file_buffer) - 1) {
        uint16_t bytes_to_read = sizeof(file_buffer) - 1 - file_len;
        if (bytes_to_read > 256) bytes_to_read = 256;
        
        uint16_t bytes_read = bytes_to_read;
        zos_err_t err = read(file_dev, file_buffer + file_len, &bytes_read);
        if (err != ERR_SUCCESS || bytes_read == 0) {
            break;
        }
        file_len += bytes_read;
    }
    file_buffer[file_len] = '\0';
    close(file_dev);
    
    Lexer lex;
    lexer_init(&lex, file_buffer, file_len);
    
    Parser p;
    parser_init(&p, &lex);
    ast_reset();
    Chunk* chunk = parser_parse(&p);
    
    if (p.has_error) {
        z_print_error(argv[1], p.error_line, p.error_column, p.error_msg);
        exit(1);
    }

    if (lex.has_error) {
        z_print_error(argv[1], lex.line, lex.column, lex.error_msg);
        exit(1);
    }
    
    compiler_init();
    if (!compiler_compile(chunk, &compiled)) {
        z_print_error(argv[1], compiled.error_line, compiled.error_column, compiled.error_msg);
        exit(1);
    }
    
    // Generate output filename
    char out_filename[128];
    if (argc > 2) {
        strcpy(out_filename, argv[2]);
    } else {
        int iter = 0;
        while(argv[1][iter] && iter < 120 && argv[1][iter] != '.') {
            out_filename[iter] = argv[1][iter];
            iter++;
        }
        out_filename[iter] = '.'; out_filename[iter+1] = 'b'; out_filename[iter+2] = 'i'; out_filename[iter+3] = 'n'; out_filename[iter+4] = '\0';
    }
    
    codegen_init();
    if(codegen_generate(&compiled, out_filename)) {
        z_print("Z80 binary generated!\n", 22);
    } else {
        z_print("Codegen error!\n", 15);
    }
    
    exit(0);
    return 0;
}

