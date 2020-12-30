#include "intr.h"
    .h8300h
    .section .text

    ; ここで定義される3つの(ハードウェア)割り込みハンドラは、ブートローダではなくOSのために存在する
    ; 割り込みが発生すると正しく SP などを設定し、OSが管理するソフト割り込みベクタに処理を移す

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
    ; H8 マイコンは er7 レジスタはスタックポインタとしても使われるとのこと
    mov.l   er7,er1
    ; スタックポインタを、割り込み用のスタックのアドレスに変更
    ; er7 と sp を使い分けている理由は不明
    ;     ここで sp を intrstack(=bootstack) にしているので、
    ;     元々のブートローダのスタックを潰してしまうのでは？
    ;     → ブートローダの main で割り込みを無効にしているので、
    ;       ブートローダ動作中にはここは呼ばれない
    ;       OS が動き出すと呼ばれることになるが、
    ;       そのときは bootstack != interstack なので問題ない
    mov.l   #_intrstack,sp
    ; er1 に退避しておいた旧スタックポインタの値を割込みのスタックに保存
    mov.l   er1,@-er7
    ; 第1引数に SOFTVEC_TYPE_SOFTERR を設定
    ; 格納先が er0 ではなく r0 なので16ビット
    ; interrupt() の第1引数の型は short(16ビット) となっている
    mov.w   #SOFTVEC_TYPE_SOFTERR,r0


    ; interrupt() 関数の呼び出し
    jsr     @_interrupt


    ; 以下は OS で割込みが処理されなかった場合に実行される
    ; (OS で処理される場合は OS 側の dispatch 関数で復元される)
    ; 旧スタックポインタの値を割込みのスタックから復元
    mov.l   @er7+,er1
    mov.l   @er1,er7

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
    mov.l   #_intrstack,sp
    mov.l   er1,@-er7
    mov.w   #SOFTVEC_TYPE_SOFTERR,r0

    jsr     @_interrupt

    mov.l   @er7+,er1
    mov.l   @er1,er7
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
    mov.l   #_intrstack,sp
    mov.l   er1,@-er7
    mov.w   #SOFTVEC_TYPE_SOFTERR,r0

    jsr     @_interrupt

    mov.l   @er7+,er1
    mov.l   @er1,er7
    mov.l   @er7+,er0
    mov.l   @er7+,er1
    mov.l   @er7+,er2
    mov.l   @er7+,er3
    mov.l   @er7+,er4
    mov.l   @er7+,er5
    mov.l   @er7+,er6
    rte
