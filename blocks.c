#include "blocks.h"
#include <assert.h>
#include <stdlib.h>

#define len(arr) (int)( sizeof(arr) / sizeof(0[arr]) )

#define K 4
#define vec(T) T __attribute__((vector_size(4*K)))

typedef union {
    vec(int)      i;
    vec(unsigned) u;
    vec(float)    f;
} Vec;

typedef struct Inst {
    void (*fn)(struct Inst const *ip, Vec *v, int end, void *ptr[]);
    int x,y,z,w,ptr,imm;
    struct Program const *call;
} Inst;

typedef struct Builder {
    int   insts,unused;
    Inst *inst;
} Builder;

typedef struct Program {
    int  slots,unused;
    Inst inst[];
} Program;


static _Bool is_pow2(int x) {
    assert(x);
    return (x & (x-1)) == 0;
}

static Val push_(Builder *b, Inst inst) {
    if (is_pow2(b->insts)) {
        b->inst = realloc(b->inst, sizeof *b->inst * (size_t)b->insts*2);
    }
    b->inst[b->insts] = inst;
    return (Val){b->insts++};
}
#define push(b,...) push_(b, (Inst){.fn=__VA_ARGS__})

Builder* builder(void) {
    Builder *b = calloc(1, sizeof *b);
    b->insts = 5;  // Slot 0 for return value, 1-4 for arguments.
    b->inst  = calloc(8, sizeof *b->inst);
    return b;
}
Val arg(struct Builder *b, int i) { (void)b; return (Val){i+1}; }

#define stage(name) static void name##_(Inst const *ip, Vec *v, int end, void *ptr[])
#define next        ip[1].fn(ip+1,v+1,end,ptr); return

stage(splat) {
    v->i = (vec(int)){0} + ip->imm;
    next;
}
Val splat(Builder *b, int imm) { return push(b, splat_, .imm=imm); }

stage(load_uniform) {
    int const *p = ptr[ip->ptr];
    v->i = (vec(int)){0} + p[ip->imm];
    next;
}
Val load_uniform(Builder *b, int ptr, int off) {
    return push(b, load_uniform_, .ptr=ptr, .imm=off);
}

stage(load_varying) {
    int const *p = ptr[ip->ptr];
    (end & (K-1)) ? __builtin_memcpy(v, p + end - 1, 1*sizeof(int))
                  : __builtin_memcpy(v, p + end - K, K*sizeof(int));
    next;
}
Val load_varying(Builder *b, int ptr) { return push(b, load_varying_, .ptr=ptr); }

stage(store_varying) {
    int *p = ptr[ip->ptr];
    (end & (K-1)) ? __builtin_memcpy(p + end - 1, v+ip->x, 1*sizeof(int))
                  : __builtin_memcpy(p + end - K, v+ip->x, K*sizeof(int));
}
void store_varying(Builder *b, int ptr, Val x) { push(b, store_varying_, x.id, .ptr=ptr); }

stage(fadd) { v->f = v[ip->x].f + v[ip->y].f             ; next; }
stage(fsub) { v->f = v[ip->x].f - v[ip->y].f             ; next; }
stage(fmul) { v->f = v[ip->x].f * v[ip->y].f             ; next; }
stage(fdiv) { v->f = v[ip->x].f / v[ip->y].f             ; next; }
stage(fmad) { v->f = v[ip->x].f * v[ip->y].f + v[ip->z].f; next; }

Val fadd(Builder *b, Val x, Val y       ) { return push(b, fadd_, x.id, y.id      ); }
Val fsub(Builder *b, Val x, Val y       ) { return push(b, fsub_, x.id, y.id      ); }
Val fmul(Builder *b, Val x, Val y       ) { return push(b, fmul_, x.id, y.id      ); }
Val fdiv(Builder *b, Val x, Val y       ) { return push(b, fdiv_, x.id, y.id      ); }
Val fmad(Builder *b, Val x, Val y, Val z) { return push(b, fmad_, x.id, y.id, z.id); }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
stage(feq) { v->i = v[ip->x].f == v[ip->y].f; next; }
stage(fne) { v->i = v[ip->x].f != v[ip->y].f; next; }
stage(flt) { v->i = v[ip->x].f <  v[ip->y].f; next; }
stage(fle) { v->i = v[ip->x].f <= v[ip->y].f; next; }
stage(fgt) { v->i = v[ip->x].f >  v[ip->y].f; next; }
stage(fge) { v->i = v[ip->x].f >= v[ip->y].f; next; }
#pragma GCC diagnostic pop

Val feq(Builder *b, Val x, Val y) { return push(b, feq_, x.id, y.id); }
Val fne(Builder *b, Val x, Val y) { return push(b, fne_, x.id, y.id); }
Val flt(Builder *b, Val x, Val y) { return push(b, flt_, x.id, y.id); }
Val fle(Builder *b, Val x, Val y) { return push(b, fle_, x.id, y.id); }
Val fgt(Builder *b, Val x, Val y) { return push(b, fgt_, x.id, y.id); }
Val fge(Builder *b, Val x, Val y) { return push(b, fge_, x.id, y.id); }

stage(band) { v->i =  v[ip->x].i & v[ip->y].i                              ; next; }
stage(bor ) { v->i =  v[ip->x].i | v[ip->y].i                              ; next; }
stage(bxor) { v->i =  v[ip->x].i ^ v[ip->y].i                              ; next; }
stage(bsel) { v->i = (v[ip->x].i & v[ip->y].i) | (~v[ip->x].i & v[ip->z].i); next; }

Val band(Builder *b, Val x, Val y       ) { return push(b, band_, x.id, y.id      ); }
Val bor (Builder *b, Val x, Val y       ) { return push(b, bor_ , x.id, y.id      ); }
Val bxor(Builder *b, Val x, Val y       ) { return push(b, bxor_, x.id, y.id      ); }
Val bsel(Builder *b, Val x, Val y, Val z) { return push(b, bsel_, x.id, y.id, z.id); }

stage(shl) { v->u = v[ip->x].u << v[ip->y].i; next; }
stage(shr) { v->u = v[ip->x].u >> v[ip->y].i; next; }
stage(sra) { v->i = v[ip->x].i >> v[ip->y].i; next; }

Val shl(Builder *b, Val x, Val y) { return push(b, shl_, x.id, y.id); }
Val shr(Builder *b, Val x, Val y) { return push(b, shr_, x.id, y.id); }
Val sra(Builder *b, Val x, Val y) { return push(b, sra_, x.id, y.id); }

stage(ret) {
    v[ip->imm] = v[ip->x];
    (void)end;
    (void)ptr;
    return;
}

Program* ret(Builder *b, Val x) {
    push(b, ret_, x.id, .imm=-b->insts);

    Program *p = calloc(1, sizeof *p + (size_t)(b->insts - 5) * sizeof *p->inst);
    p->slots = b->insts;

    Inst *inst = p->inst;
    for (int i = 5; i < b->insts; i++) {
        *inst = b->inst[i];
        inst->x -= i;
        inst->y -= i;
        inst->z -= i;
        inst->w -= i;
        inst++;
    }

    free(b->inst);
    free(b);
    return p;
}

void execute(Program const *p, int n, void *ptr[]) {
    Vec scratch[4096 / sizeof(Vec)];
    Vec *v = p->slots <= len(scratch) ? scratch
                                      : calloc((size_t)p->slots, sizeof *v);
    for (int i = 0; i < n/K*K; i += K) { p->inst->fn(p->inst,v+5,i+K,ptr); }
    for (int i = n/K*K; i < n; i += 1) { p->inst->fn(p->inst,v+5,i+1,ptr); }
    if (v != scratch) {
        free(v);
    }
}

stage(call) {
    Vec scratch[4096 / sizeof(Vec)];
    Program const *p = ip->call;
    Vec *cv = p->slots <= len(scratch) ? scratch
                                       : calloc((size_t)p->slots, sizeof *cv);
    cv[1] = v[ip->x];
    cv[2] = v[ip->y];
    cv[3] = v[ip->z];
    cv[4] = v[ip->w];
    p->inst->fn(p->inst,cv+5,end,ptr);
    *v = cv[0];
    if (cv != scratch) {
        free(cv);
    }
    next;
}
Val call(Builder *b, Program const *p, Val x, Val y, Val z, Val w) {
    return push(b, call_, x.id,y.id,z.id,w.id, .call=p);
}
