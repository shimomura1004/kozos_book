#include "defines.h"
#include "kozos.h"
#include "interrupt.h"
#include "lib.h"

// 初期スレッドで実行される関数
// init プロセスみたいなもの？
static int start_threads(int argc, char *argv[])
{
    kz_run(consdrv_main, "test11_1", 1, 0x200, 0, NULL);
    kz_run(command_main, "test11_2", 8, 0x200, 0, NULL);

    // この最初のプロセスの優先度を下げてアイドルスレッドに移行する
    kz_chpri(15);

    // ここで割込み(ハードウェア割込み、シリアルとか)を有効化
    // ソフト割込み(trapa とか)は元々有効になっている
    INTR_ENABLE;

    while (1) {
        // このスレッドに実行が戻ってきたらスリープする
        // 今の段階だとスリープすると戻ってこない
        asm volatile ("sleep");
    }

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
    // 立ち上げ時は優先度最高(0)で割込み禁止状態
    // その後 start_threads 関数内で優先度を最低(15)にしアイドルスレッドになる
    kz_start(start_threads, "idle", 0, 0x100, 0, NULL);

    // ここには戻らない

    return 0;
}