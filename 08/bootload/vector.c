#include "defines.h"

extern void start(void);        // スタートアップ
extern void intr_softerr(void); // ソフトウェアエラー
extern void intr_syscall(void); // システムコール
extern void intr_serintr(void); // シリアル割込み

// リンカスクリプトで適切な位置(メモリ空間の先頭)に配置される
// 割込みが発生したら、まずブートローダが設定したハンドラが呼び出される
// そこから割込みの種類に応じてソフト的に割込み処理を切り替える
void (*vectors[])(void) = {
    start, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    intr_syscall, intr_softerr, intr_softerr, intr_softerr,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    // SCI0 の割込みベクタ
    intr_serintr, intr_serintr, intr_serintr, intr_serintr, 
    // SCI1 の割込みベクタ
    intr_serintr, intr_serintr, intr_serintr, intr_serintr, 
    // SCI2 の割込みベクタ
    intr_serintr, intr_serintr, intr_serintr, intr_serintr, 
};
