#ifndef _INTERRUPT_H_INCLUDED_
#define _INTERRUPT_H_INCLUDED_

// softvec はリンカスクリプトで定義してあるもの
extern char softvec;
#define SOFTVEC_ADDR (&softvec)

// ソフトウェア割込みベクタの種類を表す型(short)
typedef short softvec_type_t;

// 割込みハンドラの型の定義
typedef void (*softvec_handler_t)(softvec_type_t type, unsigned long sp);

// ソフトウェア割込みベクタのアドレス
#define SOFTVECS ((softvec_handler_t *)SOFTVEC_ADDR)

// 割込みを有効化・無効化する
// CCRレジスタの上位2ビットを0にすると割込み有効化
// CCRレジスタの上位2ビットを1にすると割込み無効化(割込み禁止)
// CCR の下位14ビットはプログラムカウンタ
// 割込み有効化・無効化はアトミックに処理できないといけない
// H8 の場合は演算のオペランドに ccr レジスタを直接指定できるので、普通の演算でアトミックになる
#define INTR_ENABLE  asm volatile ("andc.b #0x3f,ccr")
#define INTR_DISABLE asm volatile ("orc.b #0xc0,ccr")

// ソフトウェア割込みベクタの初期化
int softvec_init(void);

// ソフトウェア割込みベクタにハンドラを設定する
int softvec_setintr(softvec_type_t type, softvec_handler_t handler);

// 共通の割込みハンドラ
void interrupt(softvec_type_t type, unsigned long sp);

#endif
