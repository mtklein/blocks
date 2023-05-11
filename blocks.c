#include "blocks.h"
#include <assert.h>
#include <stdlib.h>

#define K 4
#define vec(T) T __attribute__((vector_size(4*K)))

typedef union {
    vec(int)      i;
    vec(unsigned) u;
    vec(float)    f;
} Vec;

typedef struct Inst {
    void (*fn)(struct Inst const *ip, Vec *v, int end, void *ptr[]);
    int x,y,z,w,ptr;
    union { int immi; unsigned immu; float immf; };
    struct Program const *call;
} Inst;

typedef struct Builder {
    int   insts,unused;
    Inst *inst;
} Builder;

typedef struct Program {
    int  insts,unused;
    Inst inst[];
} Program;


static _Bool is_pow2_or_zero(int x) {
    return (x & (x-1)) == 0;
}

static Val push_(Builder *b, Inst inst) {
    if (is_pow2_or_zero(b->insts)) {
        b->inst = realloc(b->inst, sizeof *b->inst * (size_t)(b->insts ? b->insts*2 : 1));
    }
    b->inst[b->insts] = inst;
    return (Val){b->insts++};
}
#define push(b,...) push_(b, (Inst){.fn=__VA_ARGS__})

Builder* builder(void) {
    Builder *b = calloc(1, sizeof *b);
    b->insts = 8;  // Slots 0-3 for return values, 4-7 for arguments.
    b->inst  = calloc((size_t)b->insts, sizeof *b->inst);
    return b;
}

Val nil(struct Builder *b) {
    (void)b;
    return (Val){0};
}

Val arg(struct Builder *b, int i) {
    (void)b;
    return (Val){i+4};
}

#define stage(name) static void name##_(Inst const *ip, Vec *v, int end, void *ptr[])
#define next        ip[1].fn(ip+1,v+1,end,ptr); return

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

stage(fadd) {
    v->f = v[ip->x].f + v[ip->y].f;
    next;
}
Val fadd(Builder *b, Val x, Val y) { return push(b, fadd_, x.id,y.id); }

stage(ret) {
    int insts = ip->immi;
    v[-insts] = v[ip->x];
    (void)end;
    (void)ptr;
    return;
}

Program* ret(Builder *b, Val x) {
    push(b, ret_, x.id, .immi=b->insts);

    Program *p = malloc(sizeof *p + (size_t)b->insts * sizeof *p->inst);
    p->insts   = 0;
    for (int i = 0; i < b->insts; i++) {
        p->inst[p->insts++] = (Inst){
            b->inst[i].fn,
            b->inst[i].x - i,
            b->inst[i].y - i,
            b->inst[i].z - i,
            b->inst[i].w - i,
            b->inst[i].ptr,
           {b->inst[i].immi},
            b->inst[i].call,
        };
    }

    free(b->inst);
    free(b);
    return p;
}

void execute(Program const *p, int n, void *ptr[]) {
    Vec *v = calloc((size_t)p->insts, sizeof *v);
    for (int i = 0; i < n/K*K; i += K) { p->inst[8].fn(p->inst+8,v+8,i+K,ptr); }
    for (int i = n/K*K; i < n; i += 1) { p->inst[8].fn(p->inst+8,v+8,i+1,ptr); }
    free(v);
}

stage(call) {
    Program const *p = ip->call;
    Vec *cv = calloc((size_t)p->insts, sizeof *v);
    cv[4] = v[ip->x];
    cv[5] = v[ip->y];
    cv[6] = v[ip->z];
    cv[7] = v[ip->w];
    p->inst[8].fn(p->inst+8,cv+8,end,ptr);
    *v = cv[0];
    next;
}
Val call(Builder *b, Program const *p, Val x, Val y, Val z, Val w) {
    return push(b, call_, x.id,y.id,z.id,w.id, .call=p);
}
