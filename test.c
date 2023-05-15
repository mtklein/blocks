#include "blocks.h"
#include <stdlib.h>

#define expect(x) if (!(x)) __builtin_trap()
#define len(arr) (int)( sizeof(arr) / sizeof(0[arr]) )

static _Bool equiv(float x, float y) {
    return (x <= y && y <= x)
        || (x != x && y != y);
}

static void test_triple(void) {
    struct Program *sum;
    {
        struct Builder *b = builder();
        Val x = arg(b,0),
            y = arg(b,1);
        sum = ret(b,fadd(b,x,y), nil,nil,nil);
    }
    struct Program *triple;
    {
        struct Builder *b = builder();
        Val x   =  arg(b,0),
            xx  = call(b,sum, x,x ,nil,nil).x,
            xxx = call(b,sum, x,xx,nil,nil).x;
        triple = ret(b,xxx, nil,nil,nil);
    }
    struct Program *map_triple;
    {
        struct Builder *b = builder();
        Val v   = load_varying(b,1),
            vvv = call(b,triple, v,nil,nil,nil).x;
        store_varying(b,0, vvv);
        map_triple = ret(b, nil,nil,nil,nil);
    }

    float x[] = {1,2,3,4,5,6,7,8,9,10},
          y[len(x)] = {0};
    execute(map_triple, len(x), (void*[]){y,x});

    for (int i = 0; i < len(x); i++) {
        expect(equiv(x[i],   (float)(i+1)));
        expect(equiv(y[i], 3*(float)(i+1)));
    }

    free(map_triple);
    free(triple);
    free(sum);
}

static void test_sort(void) {
    struct Program *min;
    {
        struct Builder *b = builder();
        Val x = arg(b,0),
            y = arg(b,1);
        min = ret(b, bsel(b, flt(b,x,y), x,y)
                   , nil,nil,nil);
    }
    struct Program *max;
    {
        struct Builder *b = builder();
        Val x = arg(b,0),
            y = arg(b,1);
        max = ret(b, bsel(b, flt(b,x,y), y,x)
                   , nil,nil,nil);
    }
    struct Program *sort;
    {
        struct Builder *b = builder();
        Val x = arg(b,0),
            y = arg(b,1);
        sort = ret(b, call(b,min,x,y,nil,nil).x
                    , call(b,max,x,y,nil,nil).x
                    , nil,nil);
    }
    struct Program *map_sort;
    {
        struct Builder *b = builder();
        Val  x = load_varying(b,0),
             y = load_varying(b,1);
        Val4 p = call(b,sort,x,y,nil,nil);
        store_varying(b,0, p.x);
        store_varying(b,1, p.y);
        map_sort = ret(b, nil,nil,nil,nil);
    }

    float x[] = {1,4,5,8,9,3,4,7,8,2},
          y[] = {2,3,6,7,1,2,5,6,9,1};
    execute(map_sort, len(x), (void*[]){x,y});

    float const want_x[] = {1,3,5,7,1,2,4,6,8,1},
                want_y[] = {2,4,6,8,9,3,5,7,9,2};
    for (int i = 0; i < len(x); i++) {
        expect(equiv(x[i], want_x[i]));
        expect(equiv(y[i], want_y[i]));
    }

    free(map_sort);
    free(sort);
    free(max);
    free(min);
}

int main(void) {
    test_triple();
    test_sort();
    return 0;
}
