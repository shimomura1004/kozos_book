#include "defines.h"
#include "intr.h"
#include "interrupt.h"

// ブートローダと共通のソースになっているが、使われる部分が異なる
// softvec_init と interrupt はブートローダから呼ばれ、
// softvec_setintr は OS 側から呼ばれる
// SOFTVECS 領域はブートローダと OS で共通のアドレスとなっており共用される

// ソフトウェア割込みベクタの初期化
int softvec_init(void)
{
    int type;
    // 割込みベクタをすべて NULL にする
    for (type = 0; type < SOFTVEC_TYPE_NUM; type++)
        softvec_setintr(type, NULL);
    return 0;
}

// ソフトウェア割込みベクタにハンドラを設定
int softvec_setintr(softvec_type_t type, softvec_handler_t handler)
{
    // 割込みベクタの中で type で指定された場所にハンドラを設定
    SOFTVECS[type] = handler;
    return 0;
}

// 共通の割込みハンドラ
// 割込みの種類に応じて対応するハンドラを呼び出す
// ブートローダが呼び出し、呼び出された先の処理は OS 内で定義される(syscall_intr とか)
void interrupt(softvec_type_t type, unsigned long sp)
{
    softvec_handler_t handler = SOFTVECS[type];
    if (handler)
        handler(type, sp);
}
