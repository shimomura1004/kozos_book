    .h8300h
    ; テキストセクションに以下を出力
    .section .text
    ; たぶん、start を外部から参照できるようにする設定
    .global _start
    .type   _start,@function
_start:
    ; スタックポインタをリンカスクリプトで定義した _stack(0xffff00) に設定
    mov.l   #_bootstack,sp
    jsr     @_main

1:
    bra 1b


; dispatch はスタートアップ処理ではないが、とりあえずここで定義
; ここではスタックからレジスタの値を復元している
; レジスタの値を退避するのはブートローダの割込みハンドラ
    .global _dispatch
#   .type   _dispatch,@function
_dispatch:
    ; dispatch 関数を呼び出すとき、スレッドのスタックポインタが引数で渡される
    mov.l   @er0,er7
    ; スタックポインタ(er7)を切り替えたあと、スタックからレジスタを復元する
    mov.l   @er7+,er0
    mov.l   @er7+,er1
    mov.l   @er7+,er2
    mov.l   @er7+,er3
    mov.l   @er7+,er4
    mov.l   @er7+,er5
    mov.l   @er7+,er6
    ; 復帰命令で PC と CCR をアトミックに復元する
    rte