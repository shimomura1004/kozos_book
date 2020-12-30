#include "defines.h"
#include "elf.h"
#include "lib.h"

struct elf_header {
    struct {
        unsigned char magic[4];     // elf のマジックナンバ
        unsigned char class;        // 32/64ビットの区別
        unsigned char format;       // エンディアン
        unsigned char version;      // elf フォーマットのバージョン
        unsigned char abi;          // OS の種別
        unsigned char abi_version;  // OS のバージョン
        unsigned char reserve[7];
    } id;
    short type;                     // ファイルの種別
    short arch;                     // CPU の種類
    long version;                   // elf 形式のバージョン
    long entry_point;               // 実行開始アドレス
    long program_header_offset;     // プログラムヘッダテーブルの位置
    long section_header_offset;     // セクションヘッダテーブルのいち
    long flags;                     // 各種フラグ
    short header_size;              // ELF ヘッダのサイズ
    short program_header_size;      // プログラムヘッダのサイズ
    short program_header_num;       // プログラムヘッダの数
    short section_header_size;      // セクションヘッダのサイズ
    short section_header_num;       // セクションヘッダの数
    short section_name_index;       // セクション名を格納するセクションのインデックス
};

struct elf_program_header {
    long type;                      // セグメントの種別
    long offset;                    // ファイル中の位置
    long virtual_addr;              // 論理アドレス(VA)
    long physical_addr;             // 物理アドレス(PA)
    long file_size;                 // ファイル中のサイズ
    long memory_size;               // メモリ上でのサイズ
    long flags;                     // 各種フラグ
    long align;                     // アラインメント
};

static int elf_check(struct elf_header *header)
{
    // まず先頭のマジックナンバをチェック
    if (memcmp(header->id.magic, "\x7f" "ELF", 4))
        return -1;

    if (header->id.class   != 1) return -1; // ELF32
    if (header->id.format  != 2) return -1; // ビッグエンディアン
    if (header->id.version != 1) return -1; // version 1
    if (header->type       != 2) return -1; // 実行ファイル
    if (header->version    != 1) return -1; // version 1

    // アーキが H8/300(46) か H8/300H(47) であることを確認
    if ((header->arch != 46) && (header->arch != 47)) return -1;

    return 0;
}

static int elf_load_program(struct elf_header *header)
{
    int i;
    struct elf_program_header *phdr;

    // プログラムヘッダの個数分(セグメントの個数分)ループする
    for (i = 0; i < header->program_header_num; i++) {
        // プログラムヘッダを取得
        phdr = (struct elf_program_header *)
            ((char *)header + header->program_header_offset +
             header->program_header_size * i);

        // ロード可能なセグメントかを確認
        if (phdr->type != 1)
            continue;
        
        // セグメントの情報を出力する
        putxval(phdr->offset,        6); puts(" ");
        putxval(phdr->virtual_addr,  8); puts(" ");
        putxval(phdr->physical_addr, 8); puts(" ");
        putxval(phdr->file_size,     5); puts(" ");
        putxval(phdr->memory_size,   5); puts(" ");
        putxval(phdr->flags,         2); puts(" ");
        putxval(phdr->align,         2); puts("\n");
    }

    return 0;
}

int elf_load(char *buf)
{
    struct elf_header *header = (struct elf_header *)buf;

    if (elf_check(header) < 0)
        return -1;
    
    if (elf_load_program(header) < 0)
        return -1;
    
    return 0;
}
