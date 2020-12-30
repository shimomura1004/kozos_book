#ifndef _KOZOS_SYSCALL_H_INCLUDED_
#define _KOZOS_SYSCALL_H_INCLUDED_

#include "defines.h"

// システムコール番号
typedef enum {
    KZ_SYSCALL_TYPE_RUN = 0,
    KZ_SYSCALL_TYPE_EXIT,
    KZ_SYSCALL_TYPE_WAIT,
    KZ_SYSCALL_TYPE_SLEEP,
    KZ_SYSCALL_TYPE_WAKEUP,
    KZ_SYSCALL_TYPE_GETID,
    KZ_SYSCALL_TYPE_CHPRI,
    KZ_SYSCALL_TYPE_KMALLOC,
    KZ_SYSCALL_TYPE_KMFREE,
} kz_syscall_type_t;

// システムコール呼び出し時のパラメータ格納域の定義
// システムコールごとに専用の構造体を用意する
typedef struct {
    union {
        struct {
            kz_func_t func;     // メイン関数
            char *name;         // スレッド名
            int priority;       // タスクの優先度
            int stacksize;      // スタックサイズ
            int argc;           // メイン関数の引数の数
            char **argv;        // メイン関数の引数
            kz_thread_id_t ret; // 戻り値
        } run;

        struct {
            int dummy;
        } exit;

        struct {
            int ret;
        } wait;

        struct {
            int ret;
        } sleep;

        struct {
            kz_thread_id_t id;
            int ret;
        } wakeup;

        struct {
            kz_thread_id_t ret;
        } getid;

        struct {
            int priority;
            int ret;
        } chpri;

        struct {
            int size;
            void *ret;
        } kmalloc;
        
        struct {
            char *p;
            int ret;
        } kmfree;
    } un;
} kz_syscall_param_t;

#endif