#ifndef _KOZOS_H_INCLUDED_
#define _KOZOS_H_INCLUDED_

#include "defines.h"
#include "syscall.h"

//--------------------------- システムコール ---------------------------
// 与えられた関数を実行するスレッドを生成し、実行する
kz_thread_id_t kz_run(kz_func_t func, char *name, int priority, int stacksize,
                      int argc, char *argv[]);
// スレッド終了
void kz_exit(void);

// カレントスレッドをレディーキューの最後尾につなぎ直す
int kz_wait(void);
// カレントスレッドをレディーキューから外す
int kz_sleep(void);
// スリープ状態のスレッドをレディーキューにつなぎ直す
int kz_wakeup(kz_thread_id_t id);
// スレッドIDを取得
kz_thread_id_t kz_getid(void);
// スレッドの優先度を変更
int kz_chpri(int priority);

//--------------------------- ライブラリ関数 ---------------------------
// 初期スレッドを作り OS の動作を開始、この関数を呼び出したら戻ってこない
void kz_start(kz_func_t func, char *name, int priority, int stacksize,
              int argc, char *argv[]);
// 致命的なエラーのときに呼び出すと、行儀よく終了できる
void kz_sysdown(void);
// システムコールを実行
void kz_syscall(kz_syscall_type_t type, kz_syscall_param_t *param);

//--------------------------- ユーザスレッド ---------------------------
int test09_1_main(int argc, char *argv[]);
int test09_2_main(int argc, char *argv[]);
int test09_3_main(int argc, char *argv[]);
extern kz_thread_id_t test09_1_id;
extern kz_thread_id_t test09_2_id;
extern kz_thread_id_t test09_3_id;

#endif
