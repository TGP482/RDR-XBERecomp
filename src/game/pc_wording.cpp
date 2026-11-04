// PC wording in the save popup (saving, loading, deleting, too few free blocks): its lines are static Flash texts,
// glyphs placed when the movie was made, so each line naming the Xbox is set again naming the PC, in all five
// languages. The popup's lines are centred, so a line keeps its middle.
#include "game/pc_wording.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "xdk/hle.h"

using namespace kernel;

namespace {

struct Swap {
    std::string_view from;
    std::string_view to;
};
constexpr Swap kSwaps[] = {
    {"your Xbox console.", "your PC."},    {"Your Xbox doesn", "Your PC doesn"},
    {"die Xbox nicht", "den PC nicht"},    {"Die Xbox hat", "Der PC hat"},
    {"la consola Xbox.", "el PC."},        {"Tu Xbox no", "Tu PC no"},
    {"la console Xbox.", "le PC."},        {"La Xbox ne", "Le PC ne"},
    {"spegnere Xbox.", "spegnere il PC."}, {"Xbox non dispone", "Il PC non dispone"},
};

// the popup font's advances per 1024 of its size, for the letters the swaps bring in
struct Advance {
    char c;
    int per_1024;
};
constexpr Advance kAdvances[] = {{'P', 604}, {'C', 712}, {'e', 448}, {'l', 256},
                                 {'n', 543}, {'r', 363}, {'I', 392}, {'i', 272}};

// static text commands, shorts: 1 font and size, 2 colour (two shorts), 3 y, 4 x, 5 a line (a count, then glyph and
// advance pairs, a glyph its character less 32), 0 ends
constexpr int16_t kEnd = 0;
constexpr int16_t kFont = 1;
constexpr int16_t kColour = 2;
constexpr int16_t kY = 3;
constexpr int16_t kX = 4;
constexpr int16_t kLine = 5;
constexpr int16_t kFirstGlyph = 32;
constexpr size_t kMaxShorts = 4096;

struct Glyph {
    int16_t glyph;
    int16_t advance;
};

int Width(const std::vector<Glyph>& line) {
    int width = 0;
    for (const Glyph& g : line) width += g.advance;
    return width;
}

// a letter's advance at the text's size, a space as the line's own; -1 when the font's is not known
int16_t AdvanceOf(char c, int16_t size, const std::vector<Glyph>& line) {
    if (c == ' ') {
        for (const Glyph& g : line) {
            if (g.glyph == ' ' - kFirstGlyph) return g.advance;
        }
        return -1;
    }
    for (const Advance& a : kAdvances) {
        if (a.c == c) return int16_t(std::lround(a.per_1024 * size / 1024.0));
    }
    return -1;
}

// the line with its swap made, the letters both words share keeping their glyphs; false when it names no Xbox
bool SwapLine(std::vector<Glyph>& line, int16_t size) {
    std::string text;
    for (const Glyph& g : line) text += char(g.glyph + kFirstGlyph);
    for (const Swap& s : kSwaps) {
        size_t at = text.find(s.from);
        if (at == std::string::npos) continue;
        size_t head = std::mismatch(s.from.begin(), s.from.end(), s.to.begin(), s.to.end()).first - s.from.begin();
        size_t tail = 0;
        size_t most = std::min(s.from.size(), s.to.size()) - head;
        while (tail < most && s.from[s.from.size() - 1 - tail] == s.to[s.to.size() - 1 - tail]) tail++;
        std::vector<Glyph> middle;
        for (char c : s.to.substr(head, s.to.size() - head - tail)) {
            int16_t advance = AdvanceOf(c, size, line);
            if (advance < 0) return false;
            middle.push_back({int16_t(c - kFirstGlyph), advance});
        }
        auto first = line.begin() + ptrdiff_t(at + head);
        first = line.erase(first, line.begin() + ptrdiff_t(at + s.from.size() - tail));
        line.insert(first, middle.begin(), middle.end());
        return true;
    }
    return false;
}

// a command's shorts with its operands, 0 for none known
size_t Length(int16_t command, int16_t count) {
    switch (command) {
        case kEnd:
            return 1;
        case kFont:
        case kColour:
            return 3;
        case kY:
        case kX:
            return 2;
        case kLine:
            return count > 0 ? 2 + size_t(count) * 2 : 0;
        default:
            return 0;
    }
}

// the text's commands through their end, empty when they do not parse
std::vector<int16_t> ReadCommands(uint32_t commands) {
    std::vector<int16_t> in;
    while (in.size() < kMaxShorts) {
        uint32_t at = commands + uint32_t(in.size()) * 2;
        int16_t command = G<int16_t>(at);
        size_t length = Length(command, G<int16_t>(at + 2));
        if (!length) return {};
        for (size_t k = 0; k < length; k++) in.push_back(G<int16_t>(at + uint32_t(k) * 2));
        if (command == kEnd) return in;
    }
    return {};
}

// the commands with their lines swapped; empty when none changes
std::vector<int16_t> Rewrite(const std::vector<int16_t>& in) {
    std::vector<int16_t> out;
    size_t x_at = 0;  // the line's x in out, 0 before one
    int16_t size = 0;
    bool changed = false;
    for (size_t i = 0; i < in.size();) {
        int16_t command = in[i];
        size_t length = Length(command, i + 1 < in.size() ? in[i + 1] : 0);
        if (command != kLine) {
            if (command == kFont) size = in[i + 2];
            if (command == kX) x_at = out.size() + 1;
            out.insert(out.end(), in.begin() + ptrdiff_t(i), in.begin() + ptrdiff_t(i + length));
            i += length;
            continue;
        }
        std::vector<Glyph> line;
        for (size_t k = i + 2; k < i + length; k += 2) line.push_back({in[k], in[k + 1]});
        int old_width = Width(line);
        if (SwapLine(line, size)) {
            changed = true;
            if (x_at) out[x_at] = int16_t(out[x_at] + (old_width - Width(line)) / 2);
        }
        out.push_back(kLine);
        out.push_back(int16_t(line.size()));
        for (const Glyph& g : line) {
            out.push_back(g.glyph);
            out.push_back(g.advance);
        }
        i += length;
    }
    return changed ? out : std::vector<int16_t>();
}

}  // namespace

uint32_t PcWordingCommands(uint32_t commands) {
    static std::unordered_map<std::string, uint32_t> swapped;  // a text's commands and their swap's, 0 for none
    std::vector<int16_t> in = ReadCommands(commands);
    if (in.empty()) return commands;
    std::string key(reinterpret_cast<const char*>(in.data()), in.size() * 2);
    auto found = swapped.find(key);
    if (found == swapped.end()) {
        std::vector<int16_t> out = Rewrite(in);
        uint32_t va = out.empty() ? 0 : mem::AllocSystem(uint32_t(out.size()) * 2);
        for (size_t i = 0; i < out.size(); i++) G<int16_t>(va + uint32_t(i) * 2) = out[i];
        found = swapped.emplace(std::move(key), va).first;
    }
    return found->second ? found->second : commands;
}
