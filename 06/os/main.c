#include "defines.h"
#include "serial.h"
#include "lib.h"

// OS は直接 RAM にロードされるので 物理アドレス = 論理アドレス
// つまり初期値のコピーは不要
// BSS 領域のクリアもブートローダがやってくれるので不要

int main(void)
{
    static char buf[32];

    puts("Hello World!\n");

    while (1) {
        if (!strncmp(buf, "echo", 4)) {
            puts(buf + 4);
            puts("\n");
        } else if (!strcmp(buf, "exit")) {
            break;
        } else {
            puts("unknown.\n");
        }
    }

    return 0;
}