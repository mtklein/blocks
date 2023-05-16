// SPDX-License-Identifier: MIT
#pragma once

struct Builder *builder(void);

typedef struct { int id; } Val;
typedef struct { Val x,y,z,w; } Val4;
static const Val nil = {0};

Val            arg(struct Builder*, int ix);
Val          splat(struct Builder*, int imm);
Val   load_uniform(struct Builder*, int ptr, int off);
Val   load_varying(struct Builder*, int ptr);
void store_varying(struct Builder*, int ptr, Val);

Val fadd(struct Builder*, Val, Val);
Val fsub(struct Builder*, Val, Val);
Val fmul(struct Builder*, Val, Val);
Val fdiv(struct Builder*, Val, Val);
Val fmad(struct Builder*, Val, Val, Val);

Val feq(struct Builder*, Val, Val);
Val fne(struct Builder*, Val, Val);
Val flt(struct Builder*, Val, Val);
Val fle(struct Builder*, Val, Val);
Val fgt(struct Builder*, Val, Val);
Val fge(struct Builder*, Val, Val);

Val band(struct Builder*, Val, Val);
Val bor (struct Builder*, Val, Val);
Val bxor(struct Builder*, Val, Val);
Val bsel(struct Builder*, Val, Val, Val);

Val shl(struct Builder*, Val, Val);
Val shr(struct Builder*, Val, Val);
Val sra(struct Builder*, Val, Val);

struct Program* ret(struct Builder*, Val,Val,Val,Val);
Val4 call(struct Builder*, struct Program const*, Val,Val,Val,Val);

void execute(struct Program const*, int n, void* ptr[]);
