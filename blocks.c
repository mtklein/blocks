#include "blocks.h"
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
    int   slots,unused;
    Inst *inst;
} Builder;

typedef struct Program {
    int  slots,unused;
    Inst inst[];
} Program;


static _Bool is_pow2_or_zero(int x) {
    return (x & (x-1)) == 0;
}

static Val push_(Builder *b, Inst inst) {
    if (is_pow2_or_zero(b->slots)) {
        b->inst = realloc(b->inst, sizeof *b->inst * (size_t)(b->slots ? b->slots*2 : 1));
    }
    b->inst[b->slots] = inst;
    return (Val){b->slots++};
}
#define push(b,...) push_(b, (Inst){.fn=__VA_ARGS__})

Builder* builder(void) {
    Builder *b = calloc(1, sizeof *b);
    push(b,NULL);  // Slots 0-3 for return values.  (Slot 0 also does double duty as nil.)
    push(b,NULL);
    push(b,NULL);
    push(b,NULL);

    push(b,NULL);  // Slots 4-7 for arguments.
    push(b,NULL);
    push(b,NULL);
    push(b,NULL);
    return b;
}
Val arg(struct Builder *b, int i) { (void)b; return (Val){i+4}; }

#define stage(name) static void name##_(Inst const *ip, Vec *v, int end, void *ptr[])
#define next(vals)  ip[1].fn(ip+1,v+vals,end,ptr); return

stage(splat) {
    v->i = (vec(int)){0} + ip->imm;
    next(1);
}
Val splat(Builder *b, int imm) { return push(b, splat_, .imm=imm); }

stage(load_uniform) {
    int const *p = ptr[ip->ptr];
    v->i = (vec(int)){0} + p[ip->imm];
    next(1);
}
Val load_uniform(Builder *b, int ptr, int off) {
    return push(b, load_uniform_, .ptr=ptr, .imm=off);
}

stage(load_varying) {
    int const *p = ptr[ip->ptr];
    (end & (K-1)) ? __builtin_memcpy(v, p + end - 1, 1*sizeof(int))
                  : __builtin_memcpy(v, p + end - K, K*sizeof(int));
    next(1);
}
Val load_varying(Builder *b, int ptr) { return push(b, load_varying_, .ptr=ptr); }

stage(store_varying) {
    int *p = ptr[ip->ptr];
    (end & (K-1)) ? __builtin_memcpy(p + end - 1, v+ip->x, 1*sizeof(int))
                  : __builtin_memcpy(p + end - K, v+ip->x, K*sizeof(int));
    next(1);  // TODO: 0?
}
void store_varying(Builder *b, int ptr, Val x) { push(b, store_varying_, x.id, .ptr=ptr); }

stage(fadd) { v->f = v[ip->x].f + v[ip->y].f             ; next(1); }
stage(fsub) { v->f = v[ip->x].f - v[ip->y].f             ; next(1); }
stage(fmul) { v->f = v[ip->x].f * v[ip->y].f             ; next(1); }
stage(fdiv) { v->f = v[ip->x].f / v[ip->y].f             ; next(1); }
stage(fmad) { v->f = v[ip->x].f * v[ip->y].f + v[ip->z].f; next(1); }

Val fadd(Builder *b, Val x, Val y       ) { return push(b, fadd_, x.id, y.id      ); }
Val fsub(Builder *b, Val x, Val y       ) { return push(b, fsub_, x.id, y.id      ); }
Val fmul(Builder *b, Val x, Val y       ) { return push(b, fmul_, x.id, y.id      ); }
Val fdiv(Builder *b, Val x, Val y       ) { return push(b, fdiv_, x.id, y.id      ); }
Val fmad(Builder *b, Val x, Val y, Val z) { return push(b, fmad_, x.id, y.id, z.id); }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
stage(feq) { v->i = v[ip->x].f == v[ip->y].f; next(1); }
stage(fne) { v->i = v[ip->x].f != v[ip->y].f; next(1); }
#pragma GCC diagnostic pop
stage(flt) { v->i = v[ip->x].f <  v[ip->y].f; next(1); }
stage(fle) { v->i = v[ip->x].f <= v[ip->y].f; next(1); }
stage(fgt) { v->i = v[ip->x].f >  v[ip->y].f; next(1); }
stage(fge) { v->i = v[ip->x].f >= v[ip->y].f; next(1); }

Val feq(Builder *b, Val x, Val y) { return push(b, feq_, x.id, y.id); }
Val fne(Builder *b, Val x, Val y) { return push(b, fne_, x.id, y.id); }
Val flt(Builder *b, Val x, Val y) { return push(b, flt_, x.id, y.id); }
Val fle(Builder *b, Val x, Val y) { return push(b, fle_, x.id, y.id); }
Val fgt(Builder *b, Val x, Val y) { return push(b, fgt_, x.id, y.id); }
Val fge(Builder *b, Val x, Val y) { return push(b, fge_, x.id, y.id); }

stage(band) { v->i =  v[ip->x].i & v[ip->y].i                              ; next(1); }
stage(bor ) { v->i =  v[ip->x].i | v[ip->y].i                              ; next(1); }
stage(bxor) { v->i =  v[ip->x].i ^ v[ip->y].i                              ; next(1); }
stage(bsel) { v->i = (v[ip->x].i & v[ip->y].i) | (~v[ip->x].i & v[ip->z].i); next(1); }

Val band(Builder *b, Val x, Val y       ) { return push(b, band_, x.id, y.id      ); }
Val bor (Builder *b, Val x, Val y       ) { return push(b, bor_ , x.id, y.id      ); }
Val bxor(Builder *b, Val x, Val y       ) { return push(b, bxor_, x.id, y.id      ); }
Val bsel(Builder *b, Val x, Val y, Val z) { return push(b, bsel_, x.id, y.id, z.id); }

stage(shl) { v->u = v[ip->x].u << v[ip->y].i; next(1); }
stage(shr) { v->u = v[ip->x].u >> v[ip->y].i; next(1); }
stage(sra) { v->i = v[ip->x].i >> v[ip->y].i; next(1); }

Val shl(Builder *b, Val x, Val y) { return push(b, shl_, x.id, y.id); }
Val shr(Builder *b, Val x, Val y) { return push(b, shr_, x.id, y.id); }
Val sra(Builder *b, Val x, Val y) { return push(b, sra_, x.id, y.id); }

stage(ret) {
    v[ip->imm+0] = v[ip->x];
    v[ip->imm+1] = v[ip->y];
    v[ip->imm+2] = v[ip->z];
    v[ip->imm+3] = v[ip->w];
    (void)end;
    (void)ptr;
    return;
}

Program* ret(Builder *b, Val x, Val y, Val z, Val w) {
    push(b, ret_, x.id, y.id, z.id, w.id, .imm=-b->slots);

    int insts = 0;
    for (int i = 0; i < b->slots; i++) {
        if (b->inst[i].fn) {
            insts++;
        }
    }

    Program *p = calloc(1, sizeof *p + (size_t)insts * sizeof *p->inst);
    p->slots = b->slots;

    Inst *inst = p->inst;
    for (int i = 0; i < b->slots; i++) {
        if (b->inst[i].fn) {
            *inst = b->inst[i];
            inst->x -= i;
            inst->y -= i;
            inst->z -= i;
            inst->w -= i;
            inst++;
        }
    }

    free(b->inst);
    free(b);
    return p;
}

void execute(Program const *p, int n, void *ptr[]) {
    Vec scratch[4096 / sizeof(Vec)];
    Vec *v = p->slots <= len(scratch) ? scratch
                                      : calloc((size_t)p->slots, sizeof *v);
    for (int i = 0; i < n/K*K; i += K) { p->inst->fn(p->inst,v+8,i+K,ptr); }
    for (int i = n/K*K; i < n; i += 1) { p->inst->fn(p->inst,v+8,i+1,ptr); }
    if (v != scratch) {
        free(v);
    }
}

stage(call) {
    Vec scratch[4096 / sizeof(Vec)];
    Program const *p = ip->call;
    Vec *cv = p->slots <= len(scratch) ? scratch
                                       : calloc((size_t)p->slots, sizeof *cv);
    cv[4] = v[ip->x];
    cv[5] = v[ip->y];
    cv[6] = v[ip->z];
    cv[7] = v[ip->w];
    p->inst->fn(p->inst,cv+8,end,ptr);
    v[0] = cv[0];
    v[1] = cv[1];
    v[2] = cv[2];
    v[3] = cv[3];
    if (cv != scratch) {
        free(cv);
    }
    next(4);
}
Val4 call(Builder *b, Program const *p, Val x, Val y, Val z, Val w) {
    return (Val4) {
        push(b, call_, x.id,y.id,z.id,w.id, .call=p),
        push(b, NULL),
        push(b, NULL),
        push(b, NULL),
    };
}
