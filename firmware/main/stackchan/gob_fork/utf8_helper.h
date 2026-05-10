// StackChan firmware fork - new file by GOB (X:@GOB_52_GOB / GitHub:GOB52)
#pragma once
#include <cstddef>
#include <string_view>

namespace stackchan::gob_fork::utf8 {

// UTF-8 1 char のバイト長を返す。lead byte の上位ビットで判定。
inline size_t char_len(unsigned char c)
{
    if ((c & 0x80) == 0x00) return 1;  // ASCII
    if ((c & 0xE0) == 0xC0) return 2;  // 2-byte
    if ((c & 0xF0) == 0xE0) return 3;  // 3-byte (CJK の大半)
    if ((c & 0xF8) == 0xF0) return 4;  // 4-byte
    return 1;                           // 不正バイト: 1 として進める
}

// UTF-8 文字列の char (= code point) 数。
inline size_t char_count(std::string_view s)
{
    size_t count = 0;
    for (size_t i = 0; i < s.size();) {
        i += char_len(static_cast<unsigned char>(s[i]));
        count++;
    }
    return count;
}

// 先頭から n_chars 文字進んだ位置の byte offset を返す。
// 文字数が足りなければ s.size() を返す。
inline size_t byte_offset_head(std::string_view s, size_t n_chars)
{
    size_t i     = 0;
    size_t count = 0;
    while (i < s.size() && count < n_chars) {
        i += char_len(static_cast<unsigned char>(s[i]));
        count++;
    }
    return i;
}

// 末尾から n_chars 文字遡った位置の byte offset を返す (= tail 開始位置)。
// 文字数が足りなければ 0 を返す。
inline size_t byte_offset_tail(std::string_view s, size_t n_chars)
{
    const size_t total = char_count(s);
    if (n_chars >= total) return 0;
    return byte_offset_head(s, total - n_chars);
}

}  // namespace stackchan::gob_fork::utf8
