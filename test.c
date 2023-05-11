#include "blocks.h"
#include "expect.h"
#include <stdlib.h>

#define len(arr) (int)( sizeof(arr) / sizeof(0[arr]) )

static _Bool equiv(float x, float y) {
    return (x <= y && y <= x)
        || (x != x && y != y);
}

int main(void) {
    struct Program *sum;
    {
        struct Builder *b = builder();
        Val x = arg(b,0),
            y = arg(b,1);
        sum = ret(b, fadd(b,x,y));
    }

    struct Program *triple;
    {
        struct Builder *b = builder();
        Val x   =  arg(b,0),
            xx  = call(b,sum, x,x ,nil(b),nil(b)),
            xxx = call(b,sum, x,xx,nil(b),nil(b));
        triple = ret(b, xxx);
    }

    struct Program *map_triple;
    {
        struct Builder *b = builder();
        Val v   = load_varying(b,1),
            vvv =         call(b,triple, v,nil(b),nil(b),nil(b));
        store_varying(b,0, vvv);
        map_triple = ret(b,nil(b));
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

    expect(1);
    return 0;
}
