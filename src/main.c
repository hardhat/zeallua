#include <stdint.h>
#include <stdbool.h>
#include "zos_sys.h"
#include "zos_vfs.h"
#include "zos_video.h"
#include "zos_time.h"
#include "zos_time.h"
#include "zos_keyboard.h"
#include "lexer.h"
#include "parser.h"

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
        z_print("Parser error!\n", 14);
    } else {
        z_print("Lua file parsed successfully!\n", 30);
    }
    
    exit(0);
    return 0;
}

