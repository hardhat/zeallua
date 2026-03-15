#ifndef ZOS_HOST_STUB_H
#define ZOS_HOST_STUB_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef int8_t zos_dev_t;
typedef uint8_t zos_err_t;

#undef O_RDONLY
#undef O_WRONLY
#undef O_RDWR
#undef O_CREAT
#undef O_TRUNC

#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2
#define O_TRUNC  4
#define O_CREAT  16

#define DEV_STDOUT 1

#ifndef ERR_SUCCESS
#define ERR_SUCCESS 0
#endif

#ifdef __linux__
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

static inline zos_dev_t zos_open(const char* name, uint8_t flags) {
    int real_f = 0;
    if ((flags & 3) == O_RDONLY) real_f = O_RDONLY;
    else if ((flags & 3) == O_WRONLY) real_f = O_WRONLY;
    else if ((flags & 3) == O_RDWR) real_f = O_RDWR;
    
    // Linux flags
    if (flags & 16) real_f |= 64; // O_CREAT on linux
    if (flags & 4) real_f |= 512; // O_TRUNC on linux
    
    // Using (open) to avoid macro expansion if it existed
    int fd = open(name, real_f, 0644);
    return (zos_dev_t)fd;
}

static inline zos_err_t zos_read(zos_dev_t dev, void* buf, uint16_t* size) {
    ssize_t r = read((int)dev, buf, (size_t)*size);
    if (r < 0) return 1;
    *size = (uint16_t)r;
    return 0;
}

static inline zos_err_t zos_write(zos_dev_t dev, const void* buf, uint16_t* size) {
    ssize_t w = write((int)dev, buf, (size_t)*size);
    if (w < 0) return 1;
    *size = (uint16_t)w;
    return 0;
}

static inline void zos_close(zos_dev_t dev) {
    close((int)dev);
}

// Map Zeal names to zos_ prefixed ones
#define open zos_open
#define read zos_read
#define write zos_write
#define close zos_close

#endif

#endif
