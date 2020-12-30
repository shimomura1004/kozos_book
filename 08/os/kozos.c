#include "defines.h"
#include "kozos.h"
#include "intr.h"
#include "interrupt.h"
#include "syscall.h"
#include "lib.h"

// TCB(task control block)の数
#define THREAD_NUM 6
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
    char *stack;

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
    char dummy[16];
} kz_thread;

// OS が管理するスレッドのレディーキュー
static struct {
    kz_thread *head;
    kz_thread *tail;
} readyque;

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

    readyque.head = current->next;
    if (readyque.head == NULL) {
        // キューが空になった場合は tail も null にする
        readyque.tail = NULL;
    }
    current->next = NULL;

    return 0;
}

// カレントスレッドをキューの末尾につなげる
static int putcurrent(void)
{
    if (current == NULL) {
        return -1;
    }

    // キューが空でなければ、末尾の TCB に接続
    if (readyque.tail) {
        readyque.tail->next = current;
    } else {
        readyque.head = current;
    }
    readyque.tail = current;

    return 0;
}

// ---------------------- スレッドの起動・終了 ----------------------
// thread_* と kz_* の関係がよくわからない…
// thread_end の存在価値は…？
// おそらく、kz_* は OS の処理、thread_* はスレッドの処理

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
static kz_thread_id_t thread_run(kz_func_t func, char *name,
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
    thp->next = NULL;
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
    *(--sp) = (uint32)thread_init;

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
                                   p->un.run.stacksize,
                                   p->un.run.argc, p->un.run.argv);
        break;
    case KZ_SYSCALL_TYPE_EXIT:
        // exit を呼ぶと TCB が消去されるので、p に戻り値を書き込んではいけない
        // アプリがシステムコールを呼び出すと、情報が current にコピーされて OS が処理する
        // KZ_SYSCALL_TYPE_EXIT の場合 current が消されるので、p の指す先は解放されている
        thread_exit();
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

// スレッドのスケジューリング
static void schedule(void)
{
    // レディーキューが空の場合(全スレッドが終了)は異常終了
    if (!readyque.head)
        kz_sysdown();

    // キューの先頭のスレッドをスケジューリングする
    // この時点ではキューから外れない
    current = readyque.head;
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
void kz_start(kz_func_t func, char *name, int stacksize,
              int argc, char *argv[])
{
    // 初期化
    current = NULL;
    readyque.head = readyque.tail = NULL;
    memset(threads, 0, sizeof(threads));
    memset(handlers, 0, sizeof(handlers));

    // 割込みハンドラの登録
    setintr(SOFTVEC_TYPE_SYSCALL, syscall_intr);
    setintr(SOFTVEC_TYPE_SOFTERR, softerr_intr);

    // システムコールは呼び出せないので、直接関数を呼び出して最初のスレッド作成
    // 戻ってくるとレディーキューに最初のスレッドが追加されている
    current = (kz_thread *)thread_run(func, name, stacksize, argc, argv);

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
    asm volatile ("trapa #0");
}
