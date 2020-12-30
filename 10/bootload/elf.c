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
        
        // 物理アドレス: 変数の初期値が格納されるアドレス
        // 論理アドレス: プログラムが実行時にアクセスするアドレス
        // プログラムが ROM に書き込まれる場合、実行時には書き換えられないので　物理 != 論理 になる

        // elf ヘッダの先頭からオフセット分だけずらした位置から、
        // 物理アドレスの場所に対象のセグメントをロードする
        memcpy((char *)phdr->physical_addr, (char *)header + phdr->offset,
               phdr->file_size);
        // RAM 領域で、セグメントをコピーしたところから後ろをゼロクリアする
        // bss　領域などは、elf ファイル上は実体は不要だが、RAM 上には領域が必要
        // よって memory_size と file_size で差がでることがある
        // ここはゼロクリアする
        // おそらく elf ファイル上に実体がない領域はセグメントの後ろ側に集められる仕様になっている
        memset((char *)phdr->physical_addr + phdr->file_size, 0,
               phdr->memory_size - phdr->file_size);
    }

    return 0;
}

// boot loader: モトローラSレコードフォーマット
// OS: ELF
// ブートローダはモトローラSレコードフォーマットで書き込まれる
// これは H8 がこのフォーマットのバイナリを期待しているため
// OS 側のフォーマットは(ブートローダが期待する形式なら)なんでもいい
char *elf_load(char *buf)
{
    struct elf_header *header = (struct elf_header *)buf;

    if (elf_check(header) < 0)
        return NULL;
    
    if (elf_load_program(header) < 0)
        return NULL;
    
    // elf ファイル内に書かれたエントリポイントのアドレスを返す
    return (char *)header->entry_point;
}
