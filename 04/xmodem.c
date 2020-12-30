#include "defines.h"
#include "serial.h"
#include "lib.h"
#include "xmodem.h"

#define XMODEM_SOH 0x01
#define XMODEM_STX 0x02
#define XMODEM_EOT 0x04
#define XMODEM_ACK 0x06
#define XMODEM_NAK 0x15
#define XMODEM_CAN 0x18
#define XMODEM_EOF 0x1a

#define XMODEM_BLOCK_SIZE 128

// ブートローダ側で、受信開始まで定期的に信号を送る
static int xmodem_wait(void)
{
    long cnt = 0;

    while(!serial_is_recv_enable(SERIAL_DEFAULT_DEVICE)) {
        if (++cnt >= 200000) {
            cnt = 0;
            serial_send_byte(SERIAL_DEFAULT_DEVICE, XMODEM_NAK);
        }
    }

    return 0;
}

// ブロック単位の受信
static int xmodem_read_block(unsigned char block_number, char *buf)
{
    unsigned char c, block_num, check_sum;
    int i;

    block_num = serial_recv_byte(SERIAL_DEFAULT_DEVICE);
    // もし要求したブロック番号と違う番号のブロックを受信してしまったらエラー
    if (block_num != block_number)
        return -1;

    // xmodem のプロトコルで、次に受信されるのはブロック番号をビット反転したもの
    // 重ねると全てのビットが1になるはず
    block_num ^= serial_recv_byte(SERIAL_DEFAULT_DEVICE);
    if (block_num != 0xff)
        return -1;

    check_sum = 0;
    // 1バイトずつ受信し buf に書き込んでいく
    for (i = 0; i < XMODEM_BLOCK_SIZE; i++) {
        c = serial_recv_byte(SERIAL_DEFAULT_DEVICE);
        *(buf++) = c;
        check_sum += c;
    }

    // 最後にチェックサムを受信し比較する
    check_sum ^= serial_recv_byte(SERIAL_DEFAULT_DEVICE);
    if (check_sum)
        return -1;

    return i;
}

long xmodem_recv(char *buf)
{
    int r, receiving = 0;
    long size = 0;
    unsigned char c, block_number = 1;

    while (1) {
        // 受信開始まで待つ
        if (!receiving)
            xmodem_wait();

        c = serial_recv_byte(SERIAL_DEFAULT_DEVICE);

        if (c == XMODEM_EOT) { // 受信終了
            serial_send_byte(SERIAL_DEFAULT_DEVICE, XMODEM_ACK);
            break;
        }
        else if (c == XMODEM_CAN) { // キャンセル
            return -1;
        }
        else if (c == XMODEM_SOH) { // データ受信を開始
            receiving++;

            // 1ブロック分のデータを受信
            r = xmodem_read_block(block_number, buf);
            if (r < 0) { // エラー
                serial_send_byte(SERIAL_DEFAULT_DEVICE, XMODEM_NAK);
            } else {
                // 1ブロック分を正しく受信したら、いったん応答を返す
                block_number++;
                size += r;
                buf  += r;
                serial_send_byte(SERIAL_DEFAULT_DEVICE, XMODEM_ACK);
            }
        } else {
            if (receiving)
                return -1;
        }
    }

    return size;
}
