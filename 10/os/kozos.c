#include "defines.h"
#include "kozos.h"
#include "intr.h"
#include "interrupt.h"
#include "syscall.h"
#include "memory.h"
#include "lib.h"

// TCB(task control block)の数
#define THREAD_NUM 6
// 優先度の個数
#define PRIORITY_NUM  16
#define THREAD_NAME_SIZE 15


// スレッドコンテキスト
// コンテキストの中身はスタックポインタのみ(汎用レジスタは各スタック内に保存されるため)
typedef struct _kz_context {
    uint32 sp;
} kz_context;

// タスクコントロールブロック(TCB)
// タスクの情報を保持する構造体で、すべてのタスクがつながったリスト構造を取る
typedef struct _kz_thread {
    struct _kz_thread *next;
    char name[THREAD_NAME_SIZE + 1];
    int priority;
    char *stack;

    // 各種フラグ
    uint32 flags;
    #define KZ_THREAD_FLAG_READY (1 << 0)

    // スレッドのスタートアップ(thread_init)に渡すパラメータ
    struct {
        kz_func_t func; // スレッドのメイン関数
        int argc;       // スレッドの argc
        char **argv;    // スレッドの argv
    } init;

    // システムコール発行時にパラメータを渡すために使う領域
    struct {
        kz_syscall_type_t type;
        kz_syscall_param_t *param;
    } syscall;

    kz_context context;

    // padding のためのダミー
    char dummy[8];
} kz_thread;

// OS が管理するスレッドのレディーキュー
// 優先度ごとに別々のキューを用意する
static struct {
    kz_thread *head;
    kz_thread *tail;
} readyque[PRIORITY_NUM];

// 現在実行中のスレッド
static kz_thread *current;
// TCB の実体
static kz_thread threads[THREAD_NUM];
// 割込みハンドラ
static kz_handler_t handlers[SOFTVEC_TYPE_NUM];

// プロトタイプ宣言のみ
void dispatch(kz_context *context);


// ---------------------- レディーキューの操作 ----------------------
// カレントスレッドをキューから外す
static int getcurrent(void)
{
    if (current == NULL) {
        return -1;
    }
    if (!(current->flags & KZ_THREAD_FLAG_READY)) {
        // カレントスレッドが ready 状態でないならばなにもせず終了
        return 1;
    }

    // カレントスレッドは必ずキューの先頭にある
    readyque[current->priority].head = current->next;
    if (readyque[current->priority].head == NULL) {
        // キューが空になった場合は tail も null にする
        readyque[current->priority].tail = NULL;
    }

    // タスクをキューから外すときに READY ビットを落とす
    current->flags &= ~KZ_THREAD_FLAG_READY;
    current->next = NULL;

    return 0;
}

// カレントスレッドをキューの末尾につなげる
static int putcurrent(void)
{
    if (current == NULL) {
        return -1;
    }
    if (current->flags & KZ_THREAD_FLAG_READY) {
        // カレントスレッドが既に ready 状態であればなにもせず終了
        return 1;
    }

    // キューが空でなければ、同じ priority のキューの末尾の TCB に接続
    if (readyque[current->priority].tail) {
        readyque[current->priority].tail->next = current;
    } else {
        readyque[current->priority].head = current;
    }
    readyque[current->priority].tail = current;
    // タスクをキューに戻すときに READY ビットを立てる
    current->flags |= KZ_THREAD_FLAG_READY;

    return 0;
}

// ---------------------- スレッドの起動・終了 ----------------------
// thread_* と kz_* の関係がよくわからない…
// thread_end の存在価値は…？ → スレッドの終了と OS としてのタスクの終了は別の概念
// おそらく、kz_* は OS の処理、thread_* はスレッドの処理

// thread_* の関数はすべて static であり外部(アプリなど)から直接使えない

// スレッドの終了
static void thread_end(void)
{
    kz_exit();
}

// スレッドのスタートアップ
// スレッドが実行されると最初に実行される
// この関数なしで、スレッドが最初に実行する関数を直接 thp->init.func に登録したものにしてもいい
// kozos の場合、コンパイラの ABI への依存をなくすためにこの関数を用意している
static void thread_init(kz_thread *thp)
{
    // スレッドが作られて実行されるときに、まずここが呼ばれる
    // thread_run で thread_init をスタックに積んでいるため 

    // thp に登録しておいたスレッドのメイン関数を呼び出す
    thp->init.func(thp->init.argc, thp->init.argv);

    // スレッドのメイン処理が終わったらスレッドを消す
    thread_end();
}

// システムコールの処理(for kz_run: スレッド起動)
// スレッドを生成する
static kz_thread_id_t thread_run(kz_func_t func, char *name, int priority,
                                 int stacksize, int argc, char *argv[])
{
    int i;
    kz_thread *thp;
    uint32 *sp;
    extern char userstack;  // リンカスクリプトで定義されたアドレス
    static char *thread_stack = &userstack;

    // 未使用の TCB を探索
    for (i = 0; i < THREAD_NUM; i++) {
        thp = &threads[i];
        if (!thp->init.func)
            break;
    }
    // TCB の空きがなければ終了
    if (i == THREAD_NUM)
        return -1;

    memset(thp, 0, sizeof(*thp));

    // TCB に情報を設定
    strcpy(thp->name, name);
    thp->next      = NULL;
    thp->priority  = priority;
    thp->flags     = 0; // 初期値では READY 状態ではない
    thp->init.func = func;
    thp->init.argc = argc;
    thp->init.argv = argv;

    // OS がスタック用に管理する領域から、要求された分だけスタックを確保して割当て
    memset(thread_stack, 0, stacksize);
    thread_stack += stacksize;
    // 確保したスタックのアドレスを TCB にセット
    thp->stack = thread_stack;

    // タスクを実行する前にスタックにデータを積んでおく
    sp = (uint32 *)thp->stack;
    // thread_init からの戻り先として thread_end を指定(スタックに積んでおく)
    // スレッド処理が終了して rte が実行されたときに thread_end に戻るようになる
    *(--sp) = (uint32)thread_end;

    // ディスパッチ時にプログラムカウンタに格納される値として thread_init を設定する
    // スレッド優先度が 0 の場合には割込み禁止スレッドとする
    // (H8 の場合 SP の上位8ビットは割込み禁止などの状態を表す領域として使われる)
    *(--sp) = (uint32)thread_init | ((uint32)(priority ? 0 : 0xc0) << 24);

    // (退避したつもりの)汎用レジスタの値を準備
    // dispatch したときにこれが復元される
    *(--sp) = 0; // ER6
    *(--sp) = 0; // ER5
    *(--sp) = 0; // ER4
    *(--sp) = 0; // ER3
    *(--sp) = 0; // ER2
    *(--sp) = 0; // ER1
    // 第1引数には thp を渡す(thread_init の引数は kz_thread*)
    *(--sp) = (uint32)thp;

    // コンテキストとしてスタックポインタを保存
    thp->context.sp = (uint32)sp;

    // システムコール(kz_run)を呼び出したスレッドをレディーキューに戻す
    putcurrent();

    // 新しく作ったスレッドをレディーキュー末尾に接続する
    current = thp;
    putcurrent();

    return (kz_thread_id_t)current;
}

// システムコールの処理(for kz_exit: スレッド終了)
static int thread_exit(void)
{
    puts(current->name);
    puts(" EXIT.\n");

    // TCB のみをクリア、スタックは再利用できない
    memset(current, 0, sizeof(*current));
    return 0;
}

// 呼び出したタスクを待ち状態にする
static int thread_wait(void)
{
    // システムコールを呼び出すとレディーキューから外れる
    // putcurrent して最後尾につなぎ直すことで他のスレッドに処理を譲る
    putcurrent();
    return 0;
}

// 呼び出したタスクをスリープ
static int thread_sleep(void)
{
    // putcurrent しないことで、レディーキューから外れてスリープ状態になる
    // 今後、キューから外れたタスクは、事前に控えておいた ID(TCB のアドレス) でアクセスする
    return 0;
}

// 指定したタスクを待ち状態にする
static int thread_wakeup(kz_thread_id_t id)
{
    // wakeup を呼んだスレッドをレディーキューに戻す
    putcurrent();

    // 指定されたスレッドをレディーキューに戻す(スリープから復帰)
    current = (kz_thread *)id;
    putcurrent();

    return 0;
}

// 呼び出したタスクの ID を返す
static kz_thread_id_t thread_getid(void)
{
    putcurrent();
    return (kz_thread_id_t)current;
}

// 呼び出したタスクの優先度を変更
static int thread_chpri(int priority)
{
    int old = current->priority;
    // 優先度を変更した上でレディーキューにつなぎ直す
    if (priority >= 0)
        current->priority = priority;
    putcurrent();
    return old;
}

static void *thread_kmalloc(int size)
{
    putcurrent();
    return kzmem_alloc(size);
}

static int thread_kmfree(char *p)
{
    kzmem_free(p);
    putcurrent();
    return 0;
}

// ---------------------- 割込みハンドラ ----------------------
static void thread_intr(softvec_type_t type, unsigned long sp);

// 割込みハンドラの登録
static int setintr(softvec_type_t type, kz_handler_t handler)
{
    // プロトタイプ宣言は関数の外側で実行しないといけない？
    // static void thread_intr(softvec_type_t type, unsigned long sp);

    // まずブートローダで設定する(CPUレベルの)割込みハンドラは、ソフトウェア割込みハンドラを呼び出す
    // ソフトウェア割込みハンドラは SOFTVEC に登録されている(SOFTVEC はリンカスクリプトで定義するアドレス)
    // SOFTVEC に登録されているのは thread_intr
    // thread_intr が、ここで登録した handler を呼び出す
    // thread_intr は OS の処理。つまり割込みが発生すると OS に処理が移るということ。

    // どんな割り込みの種類に対しても、ソフトウェア割込みベクタに thread_intr を登録
    // thread_intr の中で handlers を呼び出す(割込みの種類によって処理が分かれる)
    softvec_setintr(type, thread_intr);

    // ハンドラを登録
    // thread_intr が割込みに対応した処理を呼び出すときに使うテーブルにハンドラをセット
    handlers[type] = handler;

    return 0;
}

// ---------------------- システムコールの呼び出し ----------------------
static void call_functions(kz_syscall_type_t type, kz_syscall_param_t *p)
{
    // システムコールの id とパラメータを指定して呼び出し
    // システムコールの実行中に current が書き換わるので注意

    switch (type) {
    case KZ_SYSCALL_TYPE_RUN:
        // 新しくスレッドを作ってキューにつなぐ、戻り値はスレッドの id
        p->un.run.ret = thread_run(p->un.run.func, p->un.run.name,
                                   p->un.run.priority, p->un.run.stacksize,
                                   p->un.run.argc, p->un.run.argv);
        break;
    case KZ_SYSCALL_TYPE_EXIT:
        // exit を呼ぶと TCB が消去されるので、p に戻り値を書き込んではいけない
        // アプリがシステムコールを呼び出すと、情報が current にコピーされて OS が処理する
        // KZ_SYSCALL_TYPE_EXIT の場合 current が消されるので、p の指す先は解放されている
        thread_exit();
        break;
    case KZ_SYSCALL_TYPE_WAIT:
        p->un.wait.ret = thread_wait();
        break;
    case KZ_SYSCALL_TYPE_SLEEP:
        p->un.sleep.ret = thread_sleep();
        break;
    case KZ_SYSCALL_TYPE_WAKEUP:
        p->un.wakeup.ret = thread_wakeup(p->un.wakeup.id);
        break;
    case KZ_SYSCALL_TYPE_GETID:
        p->un.getid.ret = thread_getid();
        break;
    case KZ_SYSCALL_TYPE_CHPRI:
        p->un.chpri.ret = thread_chpri(p->un.chpri.priority);
        break;
    case KZ_SYSCALL_TYPE_KMALLOC:
        p->un.kmalloc.ret = thread_kmalloc(p->un.kmalloc.size);
        break;
    case KZ_SYSCALL_TYPE_KMFREE:
        p->un.kmfree.ret = thread_kmfree(p->un.kmfree.p);
        break;
    default:
        break;
    }
}

// システムコールの処理
static void syscall_proc(kz_syscall_type_t type, kz_syscall_param_t *p)
{
    // システムコールを呼び出したスレッドをキューから外したあとで、システムコールを実行する
    // システムコールを呼び出すだけで current なスレッドがキューから外れてしまう
    // 呼び出したタスクをそのまま実行したい場合は、自分で putcurrent を呼び出す必要がある
    // 今のところシステムコールは run(スレッド作成)か exit(スレッド終了)しかなく、
    // run では putcurrent しているので問題ない
    getcurrent();
    call_functions(type, p);
}

// ---------------------- 割込み処理 ----------------------
// kozos は組み込み OS であり、一定時間ごとのスレッド切り替えは行わない

// スレッドのスケジューリング(キューごとの優先度を考慮する)
static void schedule(void)
{
    int i;

    // 優先度の高いレディーキューから順番に見て、動作可能なスレッドを探す
    for (i = 0; i < PRIORITY_NUM; i++) {
        // キューにタスクがつながっていれば探索終了
        if (readyque[i].head)
            break;
    }
    // すべてのレディーキューが空の場合(全スレッドが終了)は異常終了
    if (i == PRIORITY_NUM)
        kz_sysdown();

    // キューの先頭のスレッドをスケジューリングする
    current = readyque[i].head;

    // カレントタスクを切り替えているだけでまだ処理自体は移っていない
    // このあと dispatch するとカレントタスクが動き出す
}

// システムコールの呼び出し
static void syscall_intr(void)
{
    syscall_proc(current->syscall.type, current->syscall.param);
}

// ソフトウェアエラーの発生
static void softerr_intr(void)
{
    puts(current->name);
    puts(" DOWN\n");

    // キューから取り出し、スレッドを終了させる
    getcurrent();
    thread_exit();
}

// 割込み処理の入り口関数
// どんな割り込みが入ってきても、ブートローダで設定した割り込みハンドラによってこの関数が呼ばれる
// 割込みが発生すると(ブートローダ内の)、アセンブラで書かれた処理によりレジスタ状態などが退避され
// さらに第1引数に割込みの種類(syscall とか err とか)、第2引数に元々の SP の値が渡される
// (また SP は割込みスタックのアドレスがセットされ、自動変数の確保でアプリのスタックが潰れることはない)
// つまりここまでの関数呼び出しでスタックが消費されることはない
// よって dispatch で処理が飛んでいってもスタックがリークすることはない
static void thread_intr(softvec_type_t type, unsigned long sp)
{
    // まず、カレントスレッドのコンテキストを保存する
    // 今のスタックポインタの位置を退避
    current->context.sp = sp;

    // type ごとにハンドラを呼び出し
    // SOFTVEC_TYPE_SYSCALL なら syscall_intr
    // SOFTVEC_TYPE_SOFTERR なら softerr_intr
    // が登録されている
    if (handlers[type])
        handlers[type]();

    // 割り込み処理を終えたら、次に動作するスレッドをスケジューリングする
    schedule();

    // current に設定された(スケジューリングされた)スレッドにディスパッチ
    // これによって OS の処理を終えてアプリに処理を戻す
    dispatch(&current->context);

    // dispatch からここへは帰ってこない
    // dispatch はアセンブラで直接書かれており、スタックポインタなどが退避されていないため戻ってこられない

    // このあたりの処理には関数という考え方がない
    // goto 命令で処理があちこちに飛んでいくようなイメージ
}

// ---------------------- 初期スレッドの起動 ----------------------

// 初期スレッドを作り OS の動作を開始する
void kz_start(kz_func_t func, char *name, int priority, int stacksize,
              int argc, char *argv[])
{
    kzmem_init();
    
    // 初期化
    current = NULL;
    memset(readyque, 0, sizeof(readyque));
    memset(threads, 0, sizeof(threads));
    memset(handlers, 0, sizeof(handlers));

    // 割込みハンドラの登録
    setintr(SOFTVEC_TYPE_SYSCALL, syscall_intr);
    setintr(SOFTVEC_TYPE_SOFTERR, softerr_intr);

    // システムコールは呼び出せないので、直接関数を呼び出して最初のスレッド作成
    // 戻ってくるとレディーキューに最初のスレッドが追加されている
    current = (kz_thread *)thread_run(func, name, priority, stacksize,
                                      argc, argv);

    // 最初のスレッドを起動
    dispatch(&current->context);

    // ここには帰ってこない
    // kz_start を呼び出したときに使ったスタックは消費したままになる
}

void kz_sysdown(void)
{
    puts("system error!\n");
    while (1)
        ;
}

void kz_syscall(kz_syscall_type_t type, kz_syscall_param_t *param)
{
    // 現在のスレッドの syscall 呼び出し領域にデータを詰めて OS を呼び出す
    current->syscall.type = type;
    current->syscall.param = param;

    // トラップ割込み発行
    // syscall_intr 経由で syscall_proc が呼び出される
    // トラップ命令が発行されると、オペランドに応じて 8~11 番の割込みが発生する
    // #0 の場合は 8 番の割込みが発生、ブートローダの vector.c によると intr_syscall が呼ばれる
    asm volatile ("trapa #0");
}
