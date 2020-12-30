#include "defines.h"
#include "interrupt.h"
#include "serial.h"
#include "xmodem.h"
#include "elf.h"
#include "lib.h"

static int init(void)
{
    // リンカスクリプトで定義したシンボルを C から使えるようにする
    extern int erodata, data_start, edata, bss_start, ebss;

    // data 領域を rom から ram にコピー
    memcpy(&data_start, &erodata, (long)&edata - (long)&data_start);
    // bss 領域を 0 で埋める
    memset(&bss_start, 0, (long)&ebss - (long)&bss_start);

    softvec_init();

    serial_init(SERIAL_DEFAULT_DEVICE);

    return 0;
}

static int dump(char *buf, long size)
{
    long i;

    if (size < 0) {
        puts("no data.\n");
        return -1;
    }
    for (i = 0; i < size; i++) {
        putxval(buf[i], 2);
        if ((i & 0xf) == 15) {
            // 16バイトごとに改行を出力
            puts("\n");
        } else {
            // 8バイトごとに空白をひとつ追加
            if ((i & 0xf) == 7)
                puts(" ");
            puts(" ");
        }
    }
    puts("\n");

    return 0;
}

static void wait()
{
    volatile long i;
    for (i = 0; i < 300000; i++)
        ;
}

int main(void)
{
    static char buf[16];
    static long size = -1;
    static unsigned char *loadbuf = NULL;

    // ロードした OS のエントリポイントアドレス
    char *entry_point;
    void (*f)(void);

    // リンカスクリプトで定義されるもの、受信したデータを置く先頭アドレス
    extern int buffer_start;

    // 割込みを無効にする
    INTR_DISABLE;

    init();

    puts("kzload (kozos boot loader) started.\n");

    while (1) {
        puts("kzload> ");
        gets(buf);

        if (!strcmp(buf, "load")) {
            // xmodem でのダウンロードを開始する
            loadbuf = (char *)(&buffer_start);
            size = xmodem_recv(loadbuf);

            // OS の受信後、少し待つ
            wait();

            if (size < 0) {
                puts("\nXMODEM receive error!\n");
            } else {
                puts("\nXMODEM receive succeeded.\n");
            }
        } else if (!strcmp(buf, "dump")) {
            puts("size: ");
            putxval(size, 0);
            puts("\n");
            dump(loadbuf, size);
        } else if (!strcmp(buf, "run")) {
            entry_point = elf_load(loadbuf);
            if (!entry_point) {
                puts("run error!\n");
            } else {
                puts("starting from entry point: ");
                putxval((unsigned long)entry_point, 0);
                puts("\n");

                // エントリポイントのアドレスを関数ポインタにキャスト
                f = (void (*)(void))entry_point;
                // ロードした OS に制御を移す、戻ってこない
                f();
            }
        } else {
            puts("unknown.\n");
        }
    }

    return 0;
}
