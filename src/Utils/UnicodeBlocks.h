#pragma once
#include <vector>
#include <string>
#include <cstdint>

struct UnicodeBlock {
    std::string name;
    uint32_t start;
    uint32_t end;
    bool enabled;
    bool supported; // Runtime check if current font(s) support this block
};

// Common Unicode blocks for font atlas generation
static const std::vector<UnicodeBlock> UNICODE_BLOCKS = {
    // Latin
    {"Basic Latin", 0x0000, 0x007F, true, true},           // ASCII
    {"Latin-1 Supplement", 0x0080, 0x00FF, false, true},
    {"Latin Extended-A", 0x0100, 0x017F, false, true},
    {"Latin Extended-B", 0x0180, 0x024F, false, true},
    
    // Digits
    {"Numerals", 0x0030, 0x0039, false, true},
    
    // Spacing & Modifiers
    {"Spacing Modifier Letters", 0x02B0, 0x02FF, false, true},
    {"Combining Diacritical Marks", 0x0300, 0x036F, false, true},
    
    // Greek & Cyrillic & Thai
    {"Greek and Coptic", 0x0370, 0x03FF, false, true},
    {"Cyrillic", 0x0400, 0x04FF, false, true},
    {"Cyrillic Supplement", 0x0500, 0x052F, false, true},
    {"Thai", 0x0E00, 0x0E7F, false, true},
    
    // Vietnamese & Latin Extensions
    {"Latin Extended Additional", 0x1E00, 0x1EFF, false, true},
    
    // Punctuation & Symbols
    {"General Punctuation", 0x2000, 0x206F, false, true},
    {"Superscripts and Subscripts", 0x2070, 0x209F, false, true},
    {"Currency Symbols", 0x20A0, 0x20CF, false, true},
    {"Letterlike Symbols", 0x2100, 0x214F, false, true},
    {"Number Forms", 0x2150, 0x218F, false, true},
    {"Arrows", 0x2190, 0x21FF, false, true},
    
    // Mathematical
    {"Mathematical Operators", 0x2200, 0x22FF, false, true},
    {"Miscellaneous Technical", 0x2300, 0x23FF, false, true},
    {"Box Drawing", 0x2500, 0x257F, false, true},
    {"Block Elements", 0x2580, 0x259F, false, true},
    {"Geometric Shapes", 0x25A0, 0x25FF, false, true},
    {"Miscellaneous Symbols", 0x2600, 0x26FF, false, true},
    
    // CJK (East Asian)
    {"CJK Symbols and Punctuation", 0x3000, 0x303F, false, true},
    {"Hiragana", 0x3040, 0x309F, false, true},
    {"Katakana", 0x30A0, 0x30FF, false, true},
    {"CJK Unified Ideographs Ext. A", 0x3400, 0x4DBF, false, true},
    {"CJK Unified Ideographs", 0x4E00, 0x9FFF, false, true},
};

// Helper to generate charset from selected blocks
inline std::vector<uint32_t> GenerateCharsetFromBlocks(const std::vector<UnicodeBlock>& blocks) {
    std::vector<uint32_t> charset;
    
    for (const auto& block : blocks) {
        if (block.enabled) {
            for (uint32_t code = block.start; code <= block.end; code++) {
                if (code >= 32) charset.push_back(code);
            }
        }
    }
    
    return charset;
}
