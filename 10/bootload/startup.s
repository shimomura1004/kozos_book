    .h8300h
    ; テキストセクションに以下を出力
    .section .text
    ; たぶん、start を外部から参照できるようにする設定
    .global _start
    .type   _start,@function
; vector.c での設定により、リセット割込み後にここにジャンプする
_start:
    ; スタートアップでは bootstack を使う
    mov.l   #_bootstack,sp
    jsr     @_main

1:
    bra 1b
