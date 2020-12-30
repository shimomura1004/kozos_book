#include "intr.h"
    .h8300h
    .section .text

    ; ソフトウェアエラー
    .global _intr_softerr
#   .type   _intr_softerr,@function
_intr_softerr:
    ; 汎用レジスタ(er0-er6)の値を手動でスタックに保存する
    ; er0-er2 は揮発性レジスタ(関数呼び出しで消える可能性あり)
    ; er0-er2 は関数の引数を渡すのに使われる
    ; 引数が4つ以上ある場合はスタックで渡される
    ; er3-er6は不揮発性レジスタ(関数を呼び出しても消えない)
    ; スタックはアドレス値が小さいほうに伸びていくので、er6から順に積んでいく
    mov.l   er6,@-er7
    mov.l   er5,@-er7
    mov.l   er4,@-er7
    mov.l   er3,@-er7
    mov.l   er2,@-er7
    mov.l   er1,@-er7
    mov.l   er0,@-er7
    ; 第2引数にスタックポインタ(er7)を設定
    mov.l   er7,er1
    ; 第1引数に SOFTVEC_TYPE_SOFTERR を設定
    ; 格納先が er0 ではなく r0 なので16ビット
    ; interrupt() の第1引数の型は short(16ビット) となっている
    mov.w   #SOFTVEC_TYPE_SOFTERR,r0
    ; interrupt() 関数の呼び出し
    jsr     @_interrupt

    ; 割込みの本体部分の処理を終えたらスタックから汎用レジスタを復元
    mov.l   @er7+,er0
    mov.l   @er7+,er1
    mov.l   @er7+,er2
    mov.l   @er7+,er3
    mov.l   @er7+,er4
    mov.l   @er7+,er5
    mov.l   @er7+,er6
    ; 割込み復帰命令
    ; スタックに保存したスタックポインタ(er7)とPCを1命令で一気に復元し、復帰
    rte

    ; システムコール
    .global _intr_syscall
#   .type   _intr_syscall,@function
_intr_syscall:
    mov.l   er6,@-er7
    mov.l   er5,@-er7
    mov.l   er4,@-er7
    mov.l   er3,@-er7
    mov.l   er2,@-er7
    mov.l   er1,@-er7
    mov.l   er0,@-er7
    mov.l   er7,er1
    mov.w   #SOFTVEC_TYPE_SOFTERR,r0
    jsr     @_interrupt
    mov.l   @er7+,er0
    mov.l   @er7+,er1
    mov.l   @er7+,er2
    mov.l   @er7+,er3
    mov.l   @er7+,er4
    mov.l   @er7+,er5
    mov.l   @er7+,er6
    rte

    ; シリアル割込み
    .global _intr_serintr
#   .type   _intr_serintr,@function
_intr_serintr:
    mov.l   er6,@-er7
    mov.l   er5,@-er7
    mov.l   er4,@-er7
    mov.l   er3,@-er7
    mov.l   er2,@-er7
    mov.l   er1,@-er7
    mov.l   er0,@-er7
    mov.l   er7,er1
    mov.w   #SOFTVEC_TYPE_SOFTERR,r0
    jsr     @_interrupt
    mov.l   @er7+,er0
    mov.l   @er7+,er1
    mov.l   @er7+,er2
    mov.l   @er7+,er3
    mov.l   @er7+,er4
    mov.l   @er7+,er5
    mov.l   @er7+,er6
    rte
