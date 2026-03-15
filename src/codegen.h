#ifndef CODEGEN_H
#define CODEGEN_H

#include "compiler.h"

void codegen_init(void);
bool codegen_generate(CompiledChunk* chunk, const char* out_filename);

#endif
