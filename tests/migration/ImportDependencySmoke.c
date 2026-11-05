/* Producer dependency admission: exact C APIs, bounded failure and corner tangents. */
#include "tiny_gltf_v3.h"
#include "mikktspace.h"
#include "Allocation.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void check(int yes,const char *why){if(!yes){fprintf(stderr,"FAIL: %s\n",why);exit(1);}}
static void u32(unsigned char *p,uint32_t value){for(int i=0;i<4;++i)p[i]=(unsigned char)(value>>(i*8));}
static size_t glb(unsigned char *out,const char *json){
    const size_t n=strlen(json), padded=(n+3)&~(size_t)3;
    memcpy(out,"glTF",4);u32(out+4,2);u32(out+8,(uint32_t)(20+padded));
    u32(out+12,(uint32_t)padded);u32(out+16,0x4e4f534a);memcpy(out+20,json,n);
    memset(out+20+n,' ',padded-n);return 20+padded;
}
static int parse(const unsigned char *bytes,size_t size,size_t budget){
    tg3_model model;tg3_error_stack errors;tg3_parse_options options;
    hz_import_memory_begin(budget);tg3_parse_options_init(&options);tg3_error_stack_init(&errors);
    options.strictness=TG3_STRICT;options.images_as_is=1;options.borrow_input_buffers=1;options.skip_extras_values=1;
    options.memory.memory_budget=budget;options.memory.max_single_alloc=budget/2;options.memory.arena_block_size=4096;
    const tg3_error_code code=tg3_parse_glb(&model,&errors,bytes,size,NULL,0,&options);
    const int accepted=code==TG3_OK && !errors.has_error && !hz_import_memory_failed();
    /* Error codes/offsets are safe diagnostics. Do not dereference upstream's
       formatted message pointers: allocation exhaustion can prevent duplication
       of a stack-local formatting buffer, even while the model arena is alive. */
    for(uint32_t i=0;i<errors.count;++i){volatile tg3_error_code c=errors.entries[i].code;(void)c;}
    check(hz_import_memory_peak()<=budget,"C dependency allocation exceeded hard cap");
    tg3_model_free(&model);tg3_error_stack_free(&errors);
    check(hz_import_memory_live()==0,"Dependency arena/error allocation leaked");
    return accepted;
}
static float tangents[6][4];
static int faces(const SMikkTSpaceContext *c){(void)c;return 2;}
static int vertices(const SMikkTSpaceContext *c,int face){(void)c;(void)face;return 3;}
static void position(const SMikkTSpaceContext *c,float *v,int f,int i){(void)c;(void)f;v[0]=i==1;v[1]=i==2;v[2]=0;}
static void normal(const SMikkTSpaceContext *c,float *v,int f,int i){(void)c;(void)f;(void)i;v[0]=0;v[1]=0;v[2]=1;}
static void uv(const SMikkTSpaceContext *c,float *v,int f,int i){(void)c;v[0]=(i==1)*(f?-1.f:1.f);v[1]=i==2;}
static void tangent(const SMikkTSpaceContext *c,const float *v,float sign,int f,int i){(void)c;memcpy(tangents[f*3+i],v,3*sizeof(float));tangents[f*3+i][3]=sign;}
int main(void){
    unsigned char input[16384],changed[16384];
    const char *valid="{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],\"nodes\":[{}]}";
    size_t n=glb(input,valid);
    check(parse(input,n,1024*1024),"Pinned TinyGLTF C API rejected a bounded valid GLB");
    for(size_t size=0;size<n;++size)check(!parse(input,size,1024*1024),"Truncated GLB accepted");
    n=glb(input,"{\"asset\":{\"version\":\"2.0\"},\"scene\":0,\"scenes\":[{\"nodes\":[999]}]}");
    check(!parse(input,n,1024*1024),"Out-of-range source index accepted");
    n=glb(input,"{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{\"byteLength\":4}],\"bufferViews\":[{\"buffer\":0,\"byteOffset\":18446744073709551615,\"byteLength\":4}]}");
    check(!parse(input,n,1024*1024),"Overflowing buffer range accepted");
    n=glb(input,valid);check(!parse(input,n,128),"Allocation exhaustion was accepted");
    uint32_t random=0x3d2026;
    for(int i=0;i<2000;++i){memcpy(changed,input,n);random=random*1664525u+1013904223u;changed[random%n]^=(unsigned char)(1u+(random>>24));(void)parse(changed,n,1024*1024);}
    hz_import_memory_begin(8*1024*1024);
    SMikkTSpaceInterface api={0};SMikkTSpaceContext context={&api,NULL};
    api.m_getNumFaces=faces;api.m_getNumVerticesOfFace=vertices;api.m_getPosition=position;
    api.m_getNormal=normal;api.m_getTexCoord=uv;api.m_setTSpaceBasic=tangent;
    check(genTangSpaceDefault(&context) && !hz_import_memory_failed(),"Pinned Mikk API failed");
    for(int f=0;f<2;++f)for(int i=0;i<3;++i){const float *v=tangents[f*3+i];check(fabsf(v[0]-(f?-1.f:1.f))<1e-5f && fabsf(v[1])<1e-5f && fabsf(v[2])<1e-5f && v[3]==(f?-1.f:1.f),"Mikk per-corner mirrored tangent/sign mismatch");}
    check(hz_import_memory_live()==0,"Mikk allocations leaked");
    puts("PASS: pinned TinyGLTF C11 bounded valid/malformed/OOM/index/range/mutation checks and Mikk per-corner mirror signs; no FS/image I/O");
    return 0;
}
