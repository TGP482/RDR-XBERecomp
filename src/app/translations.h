// Text the settings panel shows in the game's other languages (translations.cpp)
#pragma once
#include <string>
#include <string_view>

namespace app {

// the game's language is English, or the phrase has no entry: nullptr. Phrases are the English text in any case.
const char16_t* Translation(std::string_view english);
// the text with each phrase that has a translation (the longest from each word start) in the game's language
std::u16string Translate(std::string_view text);

}  // namespace app
