#ifndef CODEGEN_H
#define CODEGEN_H

#include "compiler.h"

void codegen_init(void);
void codegen_set_verbose(bool verbose);
bool codegen_is_verbose(void);
bool codegen_generate(CompiledChunk* chunk, const char* out_filename);

#endif
