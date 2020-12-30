#ifndef _KOZOS_SYSCALL_H_INCLUDED_
#define _KOZOS_SYSCALL_H_INCLUDED_

#include "defines.h"

// システムコール番号
typedef enum {
    KZ_SYSCALL_TYPE_RUN = 0,
    KZ_SYSCALL_TYPE_EXIT,
} kz_syscall_type_t;

// システムコール呼び出し時のパラメータ格納域の定義
typedef struct {
    union {
        // kz_run 用のパラメータ
        struct {
            kz_func_t func;     // メイン関数
            char *name;         // スレッド名
            int stacksize;      // スタックサイズ
            int argc;           // メイン関数の引数の数
            char **argv;        // メイン関数の引数
            kz_thread_id_t ret; // 戻り値
        } run;

        // kz_exit 用のパラメータ
        struct {
            int dummy;
        } exit;
    } un;
} kz_syscall_param_t;

#endif