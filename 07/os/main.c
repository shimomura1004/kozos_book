#include "defines.h"
#include "intr.h"
#include "interrupt.h"
#include "serial.h"
#include "lib.h"

// 割込みハンドラ
static void intr(softvec_type_t type, unsigned long sp)
{
    int c;
    static char buf[32];
    static int len;

    // 受信割込みが入ったので、コンソールから1文字受信する
    c = getc();

    // 改行がきたら処理、それ以外はバッファにためるだけ
    if (c != '\n') {
        buf[len++] = c;
    } else {
        buf[len++] = '\0';
        if (!strncmp(buf, "echo", 4)) {
            puts(buf + 4);
            puts("\n");
        } else {
            puts("unknown.\n");
        }
        puts("> ");
        len = 0;
    }
}

int main(void)
{
    // ブートローダでも割込みを無効にしているが、念のため OS 側でも無効化
    INTR_DISABLE;

    puts("kozos boot succeed!\n");

    // シリアル系の割込みが入ったらすべて intr が呼び出されるように設定
    softvec_setintr(SOFTVEC_TYPE_SERINTR, intr);
    // シリアル受信割込みを有効化
    serial_intr_recv_enable(SERIAL_DEFAULT_DEVICE);

    puts("> ");

    // ここで CPU 全体の割込みを有効化
    INTR_ENABLE;
    
    while(1) {
        // 省電力モードに移行、あとはすべて割込みベースで処理する
        asm volatile ("sleep");
    }

    return 0;
}