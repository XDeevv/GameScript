/*
    see copyright notice in GameScript.h
*/
#include "GSpcheader.h"
#ifndef GS_EXCLUDE_DEFAULT_MEMFUNCTIONS
void *GS_vm_malloc(GSUnsignedInteger size){ return malloc(size); }

void *GS_vm_realloc(void *p, GSUnsignedInteger GS_UNUSED_ARG(oldsize), GSUnsignedInteger size){ return realloc(p, size); }

void GS_vm_free(void *p, GSUnsignedInteger GS_UNUSED_ARG(size)){ free(p); }
#endif

