#include "defines.h"
#include "kozos.h"
#include "intr.h"
#include "interrupt.h"
#include "serial.h"
#include "lib.h"
#include "consdrv.h"

#define CONS_BUFFER_SIZE 24

// (シリアルポートではなく)コンソールを管理するための構造体
static struct consreg {
    kz_thread_id_t id;  // コンソールを利用するスレッド
    int index;          // 利用するシリアルの番号(0-2)

    char *send_buf;     // 送信バッファ
    char *recv_buf;     // 受信バッファ
    int send_len;       // 送信バッファ中のデータサイズ
    int recv_len;       // 受信バッファ中のデータサイズ

    long dummy[3];
} consreg[CONSDRV_DEVICE_NUM];

// send_char と send_string は割込みハンドラとスレッドの両方から呼ばれる実装になっている
// 共通の資源である送信バッファを操作しているので、割込み禁止状態で呼び出して排他制御する

// 送信バッファの先頭1文字を送信
static void send_char(struct consreg *cons)
{
    int i;
    serial_send_byte(cons->index, cons->send_buf[0]);
    // 1文字分前につめる
    cons->send_len--;
    for (i = 0; i < cons->send_len; i++)
        cons->send_buf[i] = cons->send_buf[i + 1];
}

// 文字列を送信バッファに書き込み、送信を開始する
static void send_string(struct consreg *cons, char *str, int len)
{
    int i;

    // 改行コードを変換しつつ、受け取った文字列を送信バッファにコピー
    for (i = 0; i < len; i++) {
        if (str[i] == '\n')
            cons->send_buf[cons->send_len++] = '\r';
        cons->send_buf[cons->send_len++] = str[i];
    }

    // 送信割込みを有効にしたあと、最初の一文字を送信して文字列の送信を開始
    // 2文字目以降は送信完了の割込みで起こされるハンドラ内で送信される
    // 1文字目の送信を開始する前に送信割込みを有効化し、
    // すべての文字の送信が終わると送信割込みを無効にするので、
    // 送信割込みが無効の場合は送信処理を行っていない
    // まだ送信していない文字列が残っているときに send_string が呼ばれると
    // 上のループで末尾に追記されるだけで特に処理は行わない
    if (cons->send_len && !serial_intr_is_send_enable(cons->index)) {
        serial_intr_send_enable(cons->index);
        send_char(cons);
    }
}

// コンソールドライバの割込みハンドラの処理内容
// 割込みハンドラから呼ばれるので、関数内では以下のライブラリ関数しか呼べないので注意
// - 再入可能な関数
// - スレッドから呼ばれない関数、またはスレッドからも呼ばれるが必ず割込み禁止状態で呼ばれる関数
// -> スレッドがライブラリ関数を呼び出しているときに割込みが発生し、
//    割込み処理内から、そのライブラリ関数に再入してしまう可能性があるため
// 非コンテキスト状態で呼ばれる(割込み処理から呼ばれる)ため、システムコールではなくサービスコールを使う
static int consdrv_intrproc(struct consreg *cons)
{
    unsigned char c;
    char *p;

    // 受信割込みの処理
    // serial_is_recv_enable は、文字が受信できる状態になると true になる
    if (serial_is_recv_enable(cons->index)) {
        // 受信割込みは1文字ずつ発生する
        c = serial_recv_byte(cons->index);
        if (c == '\r')
            c = '\n';
        
        // エコーバック処理(受信した文字をそのまま帰す)
        send_string(cons, &c, 1);

        if (cons->id) {
            if (c != '\n') {
                // 受信したものが改行文字でなければ受信バッファに入れる
                cons->recv_buf[cons->recv_len++] = c;
            } else {
                // 改行文字がきたらバッファの内容をコマンド処理スレッドに通知する
                // システムコールではなくサービスコールを使っていることに注意
                p = kx_kmalloc(CONS_BUFFER_SIZE);
                memcpy(p, cons->recv_buf, cons->recv_len);
                kx_send(MSGBOX_ID_CONSINPUT, cons->recv_len, p);
                // 受信バッファをクリア
                cons->recv_len = 0;
            }
        }
    }

    // 送信割込みの処理
    // serial_is_send_enable は、送信が可能な状態になると true になる
    if (serial_is_send_enable(cons->index)) {
        // シリアルを使うスレッドがいない、もしくはデータがないならば送信処理を終了
        if (!cons->id || !cons->send_len) {
            // これ以降、送信割込みは受け取らない(そもそも送信しないので発生しないはず)
            serial_intr_send_disable(cons->index);
        } else {
            // 送信データがあるならば1文字送信する
            send_char(cons);
        }
    }

    return 0;
}

// 割込みハンドラ
static void consdrv_intr(void)
{
    int i;
    struct consreg *cons;

    // すべてのシリアルデバイスについて、送受信の割込みが発生しているなら処理関数を呼ぶ
    for (i = 0; i < CONSDRV_DEVICE_NUM; i++) {
        cons = &consreg[i];
        if (cons->id){
            if (serial_is_send_enable(cons->index) ||
                serial_is_recv_enable(cons->index))
                consdrv_intrproc(cons);
        }
    }
}

static int consdrv_init(void)
{
    memset(consreg, 0, sizeof(consreg));
    return 0;
}

// スレッドからの要求を処理する
static int consdrv_command(struct consreg *cons, kz_thread_id_t id,
                           int index, int size, char *command)
{
    // 受信したデータの最初の1文字がコマンド
    switch (command[0]) {
    // コンソールドライバの使用を開始
    case CONSDRV_CMD_USE:
        // このドライバを使うスレッドの ID を控える
        cons->id = id;
        // 2文字目にASCII文字として使うシリアルの番号が書かれている
        cons->index = command[1] - '0';
        cons->send_buf = kz_kmalloc(CONS_BUFFER_SIZE);
        cons->recv_buf = kz_kmalloc(CONS_BUFFER_SIZE);
        cons->send_len = 0;
        cons->recv_len = 0;
        serial_init(cons->index);
        // シリアル受信割込みを有効化する
        serial_intr_recv_enable(cons->index);
        break;
    // コンソールへの文字列出力
    case CONSDRV_CMD_WRITE:
        // send_string を使うため、割込み禁止にして排他する
        INTR_DISABLE;
        send_string(cons, command + 1, size - 1);
        INTR_ENABLE;
        break;
    default:
        break;
    }

    return 0;
}

// p[0] に入っているのは管理DBの番号(コンソールの番号)
// consreg は長さ1の配列なので、現状だと1個しかないので決め打ちとなっている
// p[2] に入っているのは使うシリアルポートの番号
int consdrv_main(int argc, char *argv[])
{
    int size, index;
    kz_thread_id_t id;
    char *p;

    consdrv_init();
    // 割込みハンドラを登録する
    kz_setintr(SOFTVEC_TYPE_SERINTR, consdrv_intr);

    while (1) {
        // 他のスレッド(コマンドスレッド)からのメッセージを受信
        // メッセージの1文字目は使うコンソールの管理番号が ASCII で入っている
        // 3文字目は使うシリアルポートの番号
        // 本文では INPUT となっているが OUTPUT の間違い
        // コマンドスレッドからメッセージを受け取って処理する
        id = kz_recv(MSGBOX_ID_CONSOUTPUT, &size, &p);
        index = p[0] - '0';
        // 指定されたシリアルデバイスで受け取ったコマンドを処理
        consdrv_command(&consreg[index], id, index, size - 1, p + 1);

        // コマンドスレッドで malloc し、こちらで free する
        kz_kmfree(p);
    }

    return 0;
}