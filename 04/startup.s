    .h8300h
    ; テキストセクションに以下を出力
    .section .text
    ; たぶん、start を外部から参照できるようにする設定
    .global _start
    .type   _start,@function
_start:
    ; スタックポインタをリンカスクリプトで定義した _stack(0xffff00) に設定
    mov.l   #_stack,sp
    jsr     @_main

1:
    bra 1b
