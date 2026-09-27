#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include "pebble.h"
#include "../src/doom/w_wad.h"
static unsigned char *bytes;
static size_t size;
static int reads;
static jmp_buf failure;
ResHandle resource_get_handle(uint32_t id) { assert(id==1); return bytes; }
size_t resource_size(ResHandle r) { return size; }
size_t resource_load_byte_range(ResHandle r,uint32_t off,uint8_t *dest,size_t n) {
    assert(off<=size && n<=size-off); memcpy(dest,bytes+off,n); ++reads; return n;
}
void app_log(uint8_t a,const char *b,int c,const char *d,...) {}
void *Z_MallocStatic(size_t n) { void *p=calloc(1,n); assert(p); return p; }
_Noreturn void I_Error(const char *s,...) { longjmp(failure,1); }
int main(int argc,char **argv) {
    assert(argc==2);
    FILE *f=fopen(argv[1],"rb"); assert(f);
    fseek(f,0,SEEK_END); size=ftell(f); rewind(f);
    bytes=malloc(size); assert(fread(bytes,1,size,f)==size); fclose(f);
    assert(setjmp(failure)==0); W_Init();
    int lines=W_GetNumForName("linedefs"), sides=W_GetNumForName("SIDEDEFS");
    assert(lines>=0 && sides>=0 && W_GetNumForName("MISSING")==-1);
    size_t n=W_LumpLength(lines); assert(n>4);
    const void *a=W_GetLumpByNum(lines);
    void *snapshot=malloc(n); memcpy(snapshot,a,n);
    const void *b=W_GetLumpByNum(sides); assert(a!=b);
    const void *g=W_GetLumpByNum(W_GetNumForName("PISGA0"));
    int before=reads;
    assert(g==W_GetLumpByNum(W_GetNumForName("PISGA0")) && before==reads);
    assert(g==W_GetLumpByNum(W_GetNumForName("PDWALL")));
    assert(a==W_GetLumpByNum(lines) && !memcmp(a,snapshot,n));
    unsigned char *copy=malloc(n); W_ReadLumpByNum(lines,copy);
    assert(!memcmp(copy,snapshot,n));
    if(!setjmp(failure)) { W_GetLumpByNum(-1); assert(!"invalid ID accepted"); }
    if(!setjmp(failure)) { volatile uint16_t bad=W_LumpLength(32767); (void)bad; assert(!"invalid ID accepted"); }
    puts("PASS: full reads, stable map pointers, scratch reuse, cache hit, invalid IDs");
    return 0;
}
