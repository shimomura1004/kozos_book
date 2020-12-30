#include "defines.h"
#include "kozos.h"
#include "lib.h"

int test09_1_main(int argc, char *argv[])
{
    puts("test09_1 started.\n");

    puts("test09_1 sleep in.\n");
    kz_sleep();
    puts("test09_1 sleep out.\n");

    puts("test09_1 chpri in.\n");
    kz_chpri(3);
    puts("test09_1 chpri out.\n");

    puts("test09_1 wait in.\n");
    kz_wait();
    puts("test09_1 wait out.\n");

    // 不正な処理により割込みが発生する状況を再現
    // トラップ命令が発行されると、オペランドに応じて 8~11 番の割込みが発生する
    // #1 の場合は 9 番の割込みが発生、ブートローダの vector.c によると intr_softerr が呼ばれる
    puts("test09_1 trap in.\n");
    asm volatile ("trapa #1");
    puts("test09_1 trap out.\n");

    puts("test09_1 exit.\n");

    return 0;
}
