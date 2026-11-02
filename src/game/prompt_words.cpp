// PC prompts in words: the five languages' tutorial and hint texts name the controls ("pull the right trigger",
// "appuyez sur la touche noire"), each phrase is swapped for its keys. The PS2 names some texts kept (L1, R2) count as
// the Xbox controls they became: L1 LT, R1 RT, L2 white, R2 black.
#include "game/prompt_words.h"

#include <algorithm>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/input.h"

namespace {

struct Phrase {
    std::u16string_view words;  // lower case
    std::u16string_view keys;   // {NAME} is a control's key
    bool exact = false;         // case kept, for the PS2 names and START
};

// longest phrases match first; a phrase matches whole words only
constexpr Phrase kPhrases[] = {
    // English: the right stick's directions in duels and climbing, then clicks, then the sticks and triggers
    {u"press up on the right thumbstick", u"press {UP}"},
    {u"press up on right thumbstick", u"press {UP}"},
    {u"press down on the right thumbstick", u"press {DOWN}"},
    {u"move forward on the right thumbstick", u"press {UP}"},
    {u"move down on the right thumbstick", u"press {DOWN}"},
    {u"press back on the right analog stick", u"press {DOWN}"},
    {u"press back", u"press {DOWN}"},
    {u"press forward", u"press {UP}"},
    {u"then forward", u"then {UP}"},
    {u"press left or right on the right thumbstick", u"move {MOUSE}"},
    {u"press left or right on the right analog stick", u"move {MOUSE}"},
    {u"clicking the right thumbstick", u"pressing {RS}"},
    {u"click the right thumbstick", u"press {RS}"},
    {u"click the left thumbstick", u"press {LS}"},
    {u"push up on the left thumbstick", u"press {FORWARD}"},
    {u"push down on the left thumbstick", u"press {BACK}"},
    {u"pulling down on the left thumbstick", u"pressing {BACK}"},
    {u"move the left thumbstick left or right", u"press {LEFT} or {RIGHT}"},
    {u"moving the left thumbstick just a little bit", u"moving while holding {WALK}"},
    {u"moving the stick just a little bit", u"moving while holding {WALK}"},
    {u"moving  the left thumbstick all the way", u"moving with {MOVE}"},
    {u"moving  the stick all the way", u"moving with {MOVE}"},
    {u"shake the left thumbstick", u"press {MOVE} quickly"},
    {u"the left thumbstick or d-pad", u"{MOVE}"},
    {u"the left control stick or d-pad", u"{MOVE}"},
    {u"left or right on the d-pad", u"{PREVIOUS} or {NEXT}"},
    {u"the d-pad", u"{PREVIOUS} or {NEXT}"},
    {u"the left thumbstick", u"{MOVE}"},
    {u"left thumbstick", u"{MOVE}"},
    {u"left analog stick", u"{MOVE}"},
    {u"the right thumbstick", u"{MOUSE}"},
    {u"right thumbstick", u"{MOUSE}"},
    {u"the right analog stick", u"{MOUSE}"},
    {u"right analog stick", u"{MOUSE}"},
    {u"the right stick", u"{MOUSE}"},
    {u"pull and hold the left trigger", u"press and hold {LT}"},
    {u"pull the left trigger", u"press {LT}"},
    {u"the left trigger", u"{LT}"},
    {u"left trigger", u"{LT}"},
    {u"pull on the right trigger", u"press {RT}"},
    {u"pull the right trigger", u"press {RT}"},
    {u"pull right trigger", u"press {RT}"},
    {u"the right trigger", u"{RT}"},
    {u"right trigger", u"{RT}"},
    {u"the the black button", u"{BLACK}"},
    {u"the black button", u"{BLACK}"},
    {u"black button", u"{BLACK}"},
    {u"the white button", u"{WHITE}"},
    {u"the start button", u"{START}"},
    {u"start button", u"{START}"},
    {u"press start button", u"press {START}"},
    {u"press start", u"press {START}"},
    {u"the START", u"{START}", true},
    {u"the the r2 button", u"{BLACK}"},
    {u"the r2 button", u"{BLACK}"},
    {u"the l1 button", u"{LT}"},
    {u"the r1 button", u"{RT}"},
    // Spanish
    {u"hacer clic en el stick analógico derecho", u"pulsar {RS}"},
    {u"hacer clic en el stick analógico izquierdo", u"pulsar {LS}"},
    {u"presiona hacia arriba en el stick analógico derecho", u"presiona {UP}"},
    {u"presiona hacia abajo en el stick analógico derecho", u"presiona {DOWN}"},
    {u"mover hacia arriba el stick analogico derecho", u"pulsar {UP}"},
    {u"mover hacia abajo el stick analógico derecho", u"pulsar {DOWN}"},
    {u"mueve hacia arriba el stick analogico derecho", u"pulsa {UP}"},
    {u"pulsa atrás en el joystick analógico derecho", u"pulsa {DOWN}"},
    {u"presionar hacia arriba en el stick analógico izquierdo", u"pulsar {FORWARD}"},
    {u"presionar hacia abajo en el stick analógico izquierdo", u"pulsar {BACK}"},
    {u"presionando hacia abajo en el stick analógico izquierdo", u"pulsando {BACK}"},
    {u"mueve el stick analógico derecho a la izquierda o la derecha", u"pulsa {LEFT} o {RIGHT}"},
    {u"si mueves ligeramente el stick analógico izquierdo", u"si mantienes {WALK} mientras te mueves"},
    {u"si mueves a fondo el stick analógico izquierdo", u"si te mueves con {MOVE}"},
    {u"agita el stick analógico izquierdo", u"pulsa {MOVE} rápidamente"},
    {u"presiona a la izquierda o la derecha en el stick analógico derecho", u"mueve {MOUSE}"},
    {u"pulsa izquierda o derecha en el joystick analógico derecho", u"mueve {MOUSE}"},
    {u"el stick analógico izquierdo o el mando de dirección", u"{MOVE}"},
    {u"a la izquierda o a la derecha en el mando de dirección", u"{PREVIOUS} o {NEXT}"},
    {u"el mando de dirección", u"{PREVIOUS} o {NEXT}"},
    {u"el stick analógico izquierdo", u"{MOVE}"},
    {u"stick analógico izquierdo", u"{MOVE}"},
    {u"joystick analógico izquierdo", u"{MOVE}"},
    {u"el stick analógico derecho", u"{MOUSE}"},
    {u"stick analógico derecho", u"{MOUSE}"},
    {u"stick analogico derecho", u"{MOUSE}"},
    {u"el joystick analógico derecho", u"{MOUSE}"},
    {u"joystick analógico derecho", u"{MOUSE}"},
    {u"el disparador izquierdo", u"{LT}"},
    {u"el disparador derecho", u"{RT}"},
    {u"el botón de disparo", u"{RT}"},
    {u"el botón negro", u"{BLACK}"},
    {u"botón negro", u"{BLACK}"},
    {u"el botón blanco", u"{WHITE}"},
    {u"el botón start", u"{START}"},
    {u"el botón l1", u"{LT}"},
    {u"el botón r1", u"{RT}"},
    {u"el botón l2", u"{WHITE}"},
    {u"el botón r2", u"{BLACK}"},
    // French
    {u"appuyez à gauche ou à droite sur le stick analogique droit", u"déplacez {MOUSE}"},
    {u"appuyant sur le stick analogique droit", u"appuyant sur {RS}"},
    {u"appuyer sur le stick analogique droit", u"appuyer sur {RS}"},
    {u"appuyez sur le stick analogique droit", u"appuyez sur {RS}"},
    {u"appuyez légèrement sur le stick analogique gauche", u"maintenez {WALK} en vous déplaçant"},
    {u"appuyez à fond sur le stick analogique gauche", u"déplacez-vous avec {MOVE}"},
    {u"appuyez en haut sur le stick analogique gauche", u"appuyez sur {FORWARD}"},
    {u"appuyez en bas sur le stick analogique gauche", u"appuyez sur {BACK}"},
    {u"appuyant en bas sur le stick analogique gauche", u"appuyant sur {BACK}"},
    {u"appuyez sur le stick analogique gauche", u"appuyez sur {LS}"},
    {u"orientez le stick analogique gauche à gauche ou à droite", u"appuyez sur {LEFT} ou {RIGHT}"},
    {u"orientez le stick analogique gauche dans tous les sens", u"appuyez rapidement sur {MOVE}"},
    {u"orientez le stick analogique droit en haut", u"appuyez sur {UP}"},
    {u"orientez le stick analogique droit en bas", u"appuyez sur {DOWN}"},
    {u"orientez le joystick analogique droit vers vous", u"appuyez sur {DOWN}"},
    {u"orientez le joystick analogique droit latéralement", u"déplacez {MOUSE}"},
    {u"orientez rapidement le joystick analogique droit", u"déplacez rapidement {MOUSE}"},
    {u"stick analogique droit en haut", u"{UP}"},
    {u"stick analogique droit en bas", u"{DOWN}"},
    {u"le stick analogique gauche ou le bouton multidirectionnel", u"{MOVE}"},
    {u"gauche ou droite du bouton multidirectionnel", u"{PREVIOUS} ou {NEXT}"},
    {u"du bouton multidirectionnel", u"de {PREVIOUS} ou {NEXT}"},
    {u"le stick analogique gauche", u"{MOVE}"},
    {u"stick analogique gauche", u"{MOVE}"},
    {u"joystick analogique gauche", u"{MOVE}"},
    {u"du stick analogique droit", u"de {MOUSE}"},
    {u"le stick analogique droit", u"{MOUSE}"},
    {u"stick analogique droit", u"{MOUSE}"},
    {u"le joystick analogique droit", u"{MOUSE}"},
    {u"joystick analogique droit", u"{MOUSE}"},
    {u"la gâchette gauche", u"{LT}"},
    {u"la gâchette droite", u"{RT}"},
    {u"gâchette droite", u"{RT}"},
    {u"la touche noire", u"{BLACK}"},
    {u"touche noire", u"{BLACK}"},
    {u"la touche blanche", u"{WHITE}"},
    {u"la touche l1", u"{LT}"},
    {u"la touche r2", u"{BLACK}"},
    // German
    {u"den rechten ministick klickst", u"{RS} drückst"},
    {u"rechten ministick klicken", u"{RS} drücken"},
    {u"linken ministick klicken", u"{LS} drücken"},
    {u"rechten ministick nach oben drücken", u"{UP} drücken"},
    {u"drücke den rechten ministick nach unten", u"drücke {DOWN}"},
    {u"drücke den rechten analog-stick nach hinten", u"drücke {DOWN}"},
    {u"drücke den rechten analog-stick nach rechts oder links", u"bewege {MOUSE}"},
    {u"drücke den rechten analog-stick in dem moment nach vorn", u"drücke {UP} in dem Moment"},
    {u"und dann nach vorn", u"und dann {UP}"},
    {u"bewege den ministick rechts nach oben", u"drücke {UP}"},
    {u"bewege den ministick rechts nach links oder nach rechts", u"bewege {MOUSE}"},
    {u"ministick rechts nach unten bewegen", u"{DOWN} drücken"},
    {u"bewege den rechten analog-stick rasch", u"bewege {MOUSE} rasch"},
    {u"bewege den linken ministick nach links oder rechts", u"drücke {LEFT} oder {RIGHT}"},
    {u"bewege den ministick links leicht", u"halte {WALK} beim Bewegen"},
    {u"bewegst du den ministick links bis zum anschlag", u"bewegst du dich mit {MOVE}"},
    {u"bewege den ministick links schnell hin und her", u"drücke {MOVE} schnell hin und her"},
    {u"bewege im hängen den ministick links nach oben", u"drücke im Hängen {FORWARD}"},
    {u"bewege im hängen den ministick links nach unten", u"drücke im Hängen {BACK}"},
    {u"indem du den ministick links nach unten bewegst", u"indem du {BACK} drückst"},
    {u"den ministick links oder drücke auf dem steuerkreuz", u"{MOVE}"},
    {u"links oder rechts auf dem steuerkreuz", u"{PREVIOUS} oder {NEXT}"},
    {u"mit dem steuerkreuz", u"mit {PREVIOUS} oder {NEXT}"},
    {u"dem ministick links", u"{MOVE}"},
    {u"den ministick links", u"{MOVE}"},
    {u"ministick links", u"{MOVE}"},
    {u"linker analog-stick", u"{MOVE}"},
    {u"dem ministick rechts", u"{MOUSE}"},
    {u"den \nministick rechts", u"{MOUSE}"},
    {u"den ministick rechts", u"{MOUSE}"},
    {u"den rechten ministick", u"{MOUSE}"},
    {u"ministick rechts", u"{MOUSE}"},
    {u"rechter analog-stick", u"{MOUSE}"},
    {u"den rechten analog-stick", u"{MOUSE}"},
    {u"ziehe den schalter links und halte ihn fest", u"halte {LT} gedrückt"},
    {u"ziehe den schalter links, halte ihn fest", u"halte {LT} gedrückt"},
    {u"ziehe den schalter rechts", u"drücke {RT}"},
    {u"den schalter rechts zu ziehen", u"{RT} zu drücken"},
    {u"mit dem schalter rechts", u"mit {RT}"},
    {u"mit dem schalter links", u"mit {LT}"},
    {u"die start-taste", u"{START}"},
    {u"die feuer-taste", u"{RT}"},
    {u"start drücken", u"{START} drücken"},
    {u"weiß drücken", u"{WHITE} drücken"},
    {u"zoom: schwarz", u"Zoom: {BLACK}"},
    {u"die l1-taste", u"{LT}"},
    {u"die r1-taste", u"{RT}"},
    {u"der r1-taste", u"{RT}"},
    {u"die l2-taste", u"{WHITE}"},
    {u"die  l2-taste", u"{WHITE}"},
    {u"die r2-taste", u"{BLACK}"},
    {u"der r2-taste", u"{BLACK}"},
    {u"r2-taste", u"{BLACK}"},
    // Italian
    {u"premi la levetta destra", u"premi {RS}"},
    {u"premere la levetta destra", u"premere {RS}"},
    {u"premi la levetta sinistra", u"premi {LS}"},
    {u"la levetta destra", u"{MOUSE}"},
    {u"la levetta sinistra", u"{MOVE}"},
    {u"il tasto direzionale", u"{PREVIOUS} o {NEXT}"},
    {u"il grilletto sinistro", u"{LT}"},
    {u"il grilletto destro", u"{RT}"},
    {u"grilletto destro", u"{RT}"},
    {u"il comando di fuoco", u"{RT}"},
    {u"il pulsante nero", u"{BLACK}"},
    {u"pulsante nero", u"{BLACK}"},
    {u"il pulsante bianco", u"{WHITE}"},
    {u"il pulsante start", u"{START}"},
    {u"pulsante start", u"{START}"},
    {u"il tasto start", u"{START}"},
    {u"il tasto l1", u"{LT}"},
    {u"il tasto r1", u"{RT}"},
    {u"il tasto l2", u"{WHITE}"},
    {u"il tasto r2", u"{BLACK}"},
    // every language
    {u"START", u"{START}", true},
    {u"L1", u"{LT}", true},
    {u"R1", u"{RT}", true},
    {u"L2", u"{WHITE}", true},
    {u"R2", u"{BLACK}", true},
};

struct Control {
    std::u16string_view name;
    input::PromptButton button;
};
constexpr Control kControls[] = {
    {u"RT", input::kPromptRightTrigger},  {u"LT", input::kPromptLeftTrigger},
    {u"RS", input::kPromptRightThumb},    {u"LS", input::kPromptLeftThumb},
    {u"BLACK", input::kPromptBlack},      {u"WHITE", input::kPromptWhite},
    {u"START", input::kPromptStart},      {u"PREVIOUS", input::kPromptDpadLeft},
    {u"NEXT", input::kPromptDpadRight},   {u"FORWARD", input::kPromptMoveForward},
    {u"BACK", input::kPromptMoveBack},    {u"LEFT", input::kPromptMoveLeft},
    {u"RIGHT", input::kPromptMoveRight},  {u"UP", input::kPromptStickUp},
    {u"DOWN", input::kPromptStickDown},   {u"WALK", input::kPromptWalk},
};
constexpr input::PromptButton kMoveKeys[] = {input::kPromptMoveForward, input::kPromptMoveLeft,
                                             input::kPromptMoveBack, input::kPromptMoveRight};
constexpr size_t kMaxCached = 4096;

char16_t Lower(char16_t c) {
    bool upper = (c >= u'A' && c <= u'Z') || (c >= 0xC0 && c <= 0xDE && c != 0xD7);
    return upper ? char16_t(c + 0x20) : c;
}

bool IsLetter(char16_t c) {
    return (c >= u'0' && c <= u'9') || (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z') || (c >= 0xC0 && c != 0xD7);
}

const std::vector<const Phrase*>& Longest() {
    static const std::vector<const Phrase*> phrases = [] {
        std::vector<const Phrase*> v;
        for (const Phrase& p : kPhrases) v.push_back(&p);
        std::stable_sort(v.begin(), v.end(), [](const Phrase* a, const Phrase* b) {
            return a->words.size() > b->words.size();
        });
        return v;
    }();
    return phrases;
}

bool Matches(const std::u16string& text, const std::u16string& lower, size_t at, const Phrase& p) {
    const std::u16string& in = p.exact ? text : lower;
    if (in.compare(at, p.words.size(), p.words.data(), p.words.size()) != 0) return false;
    size_t end = at + p.words.size();
    return (at == 0 || !IsLetter(text[at - 1])) && (end == text.size() || !IsLetter(text[end]));
}

// the text with its phrases as {NAME}s, empty when it has none
std::u16string Template(const std::u16string& text) {
    std::u16string lower(text);
    std::transform(lower.begin(), lower.end(), lower.begin(), Lower);
    std::u16string out;
    bool changed = false;
    for (size_t i = 0; i < text.size();) {
        const Phrase* match = nullptr;
        for (const Phrase* p : Longest()) {
            if (Matches(text, lower, i, *p)) {
                match = p;
                break;
            }
        }
        if (!match) {
            out += text[i++];
            continue;
        }
        size_t start = out.size();
        out += match->keys;
        bool capital = text[i] != lower[i];
        if (capital && out[start] >= u'a' && out[start] <= u'z') out[start] = char16_t(out[start] - 0x20);
        i += match->words.size();
        changed = true;
    }
    return changed ? out : std::u16string();
}

std::u16string Keys(std::u16string_view name) {
    std::string keys;
    if (name == u"MOUSE") keys = "MOUSE";
    if (name == u"MOVE") {
        for (input::PromptButton b : kMoveKeys) keys += (keys.empty() ? "" : " ") + input::PromptKey(b);
    }
    for (const Control& c : kControls) {
        if (c.name == name) keys = input::PromptKey(c.button);
    }
    if (keys.empty()) keys = "?";
    return u"(" + std::u16string(keys.begin(), keys.end()) + u")";
}

std::u16string Expand(const std::u16string& keys) {
    std::u16string out;
    for (size_t i = 0; i < keys.size(); i++) {
        size_t close = keys[i] == u'{' ? keys.find(u'}', i) : std::u16string::npos;
        if (close == std::u16string::npos) {
            out += keys[i];
            continue;
        }
        out += Keys(std::u16string_view(keys).substr(i + 1, close - i - 1));
        i = close;
    }
    return out;
}

}  // namespace

bool WithControlKeys(std::u16string& text) {
    static std::unordered_map<std::u16string, std::u16string> templates;
    auto found = templates.find(text);
    if (found == templates.end()) {
        if (templates.size() >= kMaxCached) templates.clear();
        found = templates.emplace(text, Template(text)).first;
    }
    if (found->second.empty()) return false;
    text = Expand(found->second);
    return true;
}
