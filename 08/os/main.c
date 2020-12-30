#include "defines.h"
#include "kozos.h"
#include "interrupt.h"
#include "lib.h"

// 初期スレッドで実行される関数
// init プロセスみたいなもの？
static int start_threads(int argc, char *argv[])
{
    // test08_1_main が、いわゆるアプリケーション
    kz_run(test08_1_main, "command", 0x100, 0, NULL);
    return 0;
}

int main(void)
{
    // ブートローダでも割込みを無効にしているが、念のため OS 側でも無効化
    INTR_DISABLE;

    // 割込みを有効にしていない
    // システムコールに使うトラップ命令は割込み禁止でも実行される
    // 割込み禁止で止まるのはデバイス割込み(シリアルとか)のみ

    puts("kozos boot succeed!\n");

    // OS の動作を開始する
    // start_threads 関数を初期スレッドとして立ち上がる
    kz_start(start_threads, "start", 0x100, 0, NULL);

    // ここには戻らない

    return 0;
}