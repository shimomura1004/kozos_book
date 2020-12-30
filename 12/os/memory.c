#include "defines.h"
#include "kozos.h"
#include "lib.h"
#include "memory.h"

// メモリブロック構造体(獲得された各領域は、先頭に以下の構造体を持っている)
typedef struct _kzmem_block {
    // メモリブロック同士はリンク構造で管理される
    struct _kzmem_block *next;
    int size;
} kzmem_block;

// メモリプール
// ブロックのサイズごとに用意する
typedef struct _kzmem_pool {
    int size;
    int num;
    kzmem_block *free;
} kzmem_pool;

static kzmem_pool pool[] = {
    // 16バイト、32バイト、64バイトの3種類のメモリプールを定義する
    { 16, 8, NULL }, { 32, 8, NULL }, { 64, 4, NULL }
};

#define MEMORY_AREA_NUM (sizeof(pool) / sizeof(*pool))

// メモリプールの初期化
static int kzmem_init_pool(kzmem_pool *p)
{
    int i;
    kzmem_block *mp;
    kzmem_block **mpp;
    extern char freearea;           // freearea はリンカスクリプトで定義される
    static char *area = &freearea;  // static なので徐々にずれていく

    // 使っていない領域の先頭を取得
    mp = (kzmem_block *)area;

    mpp = &p->free;
    // pool[] で定義した数だけループしてメモリブロックを確保する
    for (i = 0; i < p->num; i++) {
        // 空き領域の先頭を指すようにポインタの値を更新し、リンク構造を作る
        *mpp = mp;
        // 確保した領域のヘッダ部分(kzmem_block)は 0 クリアする
        memset(mp, 0, sizeof(*mp));
        // ヘッダにメモリブロックのサイズを記録
        mp->size = p->size;
        // 次のループに備えて、今作ったヘッダの next が指す先を mpp に入れる
        mpp = &(mp->next);
        // メモリブロックの大きさ分、ポインタを先に進めることで、領域を確保する
        // 確保したメモリブロックの中にヘッダが含まれるため、使用できる領域は少し小さくなる
        mp = (kzmem_block *)((char *)mp + p->size);
        area += p->size;
    }

    return 0;
}

int kzmem_init(void)
{
    int i;

    // メモリブロックのサイズごとに初期化を実施
    for (i = 0; i < MEMORY_AREA_NUM; i++) {
        kzmem_init_pool(&pool[i]);
    }

    return 0;
}

void *kzmem_alloc(int size)
{
    int i;
    kzmem_block *mp;
    kzmem_pool *p;

    for (i = 0; i < MEMORY_AREA_NUM; i++) {
        p = &pool[i];

        // 確保しようとしているサイズがブロックのサイズよりも小さいか(ヘッダ分は差し引く)
        if (size <= p->size - sizeof(kzmem_block)) {
            if (p->free == NULL) {
                // メモリが枯渇している
                kz_sysdown();
                return NULL;
            }

            // 先頭のブロックをリストから外し、割り当て
            mp = p->free;
            p->free = p->free->next;
            // 割り当てたメモリブロックの next ポインタはクリアしておく
            mp->next = NULL;

            // mp の型は kzmem_block* なので、アドレスに 1 を足すと
            // sizeof(kzmem_block) 分加算され、ヘッダ直後のアドレスが帰ってくる
            return mp + 1;
        }
    }

    // 最大のメモリブロックより大きな領域を確保しようとしたらエラー
    kz_sysdown();
    return NULL;
}

void kzmem_free(void *mem)
{
    int i;
    kzmem_block *mp;
    kzmem_pool *p;

    // 渡されたアドレスの直前にあるヘッダにアクセス
    mp = ((kzmem_block *)mem - 1);

    for (i = 0; i < MEMORY_AREA_NUM; i++) {
        p = &pool[i];

        // 解放したいメモリブロックと同じサイズのプールを探す
        if (mp->size == p->size) {
            // 解放するブロックはリストの先頭に入れる
            // 今 free が指しているところを解放するブロックにコピー
            mp->next = p->free;
            // free は新しく解放したブロックを指す
            p->free = mp;

            return;
        }
    }

    kz_sysdown();
}
