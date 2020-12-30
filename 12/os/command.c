#include "defines.h"
#include "kozos.h"
#include "consdrv.h"
#include "lib.h"

// コンソールドライバの使用開始のためのメッセージを送信
static void send_use(int index)
{
    char *p;
    p = kz_kmalloc(3);
    // 0番目のコンソール
    p[0] = '0';
    p[1] = CONSDRV_CMD_USE;
    // index 番目のシリアルポート
    p[2] = '0' + index;
    kz_send(MSGBOX_ID_CONSOUTPUT, 3, p);
}

// コンソールへの文字列出力をドライバにメッセージ送信
static void send_write(char *str)
{
    char *p;
    int len;
    len = strlen(str);
    p = kz_kmalloc(len + 2);
    // 0番目のコンソール
    p[0] = '0';
    p[1] = CONSDRV_CMD_WRITE;
    memcpy(&p[2], str, len);
    kz_send(MSGBOX_ID_CONSOUTPUT, len + 2, p);
}

int command_main(int argc, char *argv[])
{
    char *p;
    int size;

    send_use(SERIAL_DEFAULT_DEVICE);

    while (1) {
        send_write("commadn> ");

        // コンソールからの受信メッセージを(コンソールドライバから)受信
        kz_recv(MSGBOX_ID_CONSINPUT, &size, &p);
        p[size] = '\0';

        if (!strncmp(p, "echo", 4)) {
            // echo コマンドの処理
            // 受信した文字列をそのまま出力する
            send_write(p + 4);
            send_write("\n");
        } else {
            send_write("unknown.\n");
        }

        // ドライバ側で malloc して送ってきた領域はこちらで free する
        kz_kmfree(p);
    }

    return 0;
}