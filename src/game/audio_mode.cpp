// The game's Audio Mode row (Sound Options) and its speakers follow the settings' AUDIO MODE, and a pick in that row
// sets AUDIO MODE.
#include <iterator>

#include "app/app.h"
#include "app/game_settings.h"
#include "audio/audio.h"
#include "xdk/hle.h"

using namespace kernel;

namespace {

// the game's speaker modes; the profile keeps its own
constexpr uint32_t kSpeakersMono = 0x1F;
constexpr uint32_t kSpeakersStereo = 0x20;
constexpr uint32_t kSpeakersSurround = 0x22;
constexpr uint32_t kProfile = 0x664260;
constexpr uint32_t kProfileSpeakers = 0x44;
// the sound options page's Audio Mode combo and its picked item, in the row's order
constexpr uint32_t kAudioModeCombo = 0x458;
constexpr uint32_t kComboPicked = 0x1274;
constexpr uint32_t kRowOutputs[] = {audio::kOutputStereo, audio::kOutputSurround, audio::kOutputMono};
constexpr uint32_t kRowSpeakers[] = {kSpeakersStereo, kSpeakersSurround, kSpeakersMono};
// the sound manager, its speaker mode thiscall(manager, mode) ret 4
constexpr uint32_t kSoundManager = 0x6675E4;
constexpr uint32_t kSetSpeakers = 0x99DC0;

uint32_t Speakers() {
    switch (app::Rdr().audio_output) {
        case audio::kOutputMono: return kSpeakersMono;
        case audio::kOutputStereo: return kSpeakersStereo;
        case audio::kOutputSurround: return kSpeakersSurround;
    }
    return audio::IsSurround() ? kSpeakersSurround : kSpeakersStereo;  // automatic: the device's
}

void UpdateProfile() {
    uint32_t profile = R32(kProfile);
    if (profile) W32(profile + kProfileSpeakers, Speakers());
}

// an AUDIO MODE change made while the page is open moves its row and the game's speakers with it
void UpdateRow(X86Context& ctx, uint32_t page) {
    uint32_t combo = R32(page + kAudioModeCombo);
    uint32_t speakers = Speakers();
    uint32_t row = 0;
    while (row + 1 < std::size(kRowSpeakers) && kRowSpeakers[row] != speakers) row++;
    if (!combo || R32(combo + kComboPicked) == row) return;
    W32(combo + kComboPicked, row);
    UpdateProfile();
    uint32_t manager = R32(kSoundManager);
    if (!manager) return;
    ctx.ecx = manager;
    CallGuest(ctx, kSetSpeakers, {speakers});
}

}  // namespace

// the sound engine's speaker mode, cdecl(mode): always the output's, from the engine's start on
X86_EXTERN_FUNC(__imp__sub_00282BB0);
X86_FUNC(sub_00282BB0) {
    W32(ctx.esp + 4, Speakers());
    UpdateProfile();
    __imp__sub_00282BB0(ctx, base);
}

// the sound options page opens, thiscall(page): its Audio Mode row reads the profile's
X86_EXTERN_FUNC(__imp__sub_000304F0);
X86_FUNC(sub_000304F0) {
    UpdateProfile();
    __imp__sub_000304F0(ctx, base);
}

// the sound options page's draw, thiscall(page)
X86_EXTERN_FUNC(__imp__sub_000303F0);
X86_FUNC(sub_000303F0) {
    uint32_t page = ctx.ecx;
    UpdateRow(ctx, page);
    ctx.ecx = page;
    __imp__sub_000303F0(ctx, base);
}

// the Audio Mode row changed, thiscall(page): the pick becomes AUDIO MODE, then the game applies it
X86_EXTERN_FUNC(__imp__sub_00030400);
X86_FUNC(sub_00030400) {
    uint32_t combo = R32(ctx.ecx + kAudioModeCombo);
    uint32_t picked = combo ? R32(combo + kComboPicked) : 0;
    if (combo && picked < std::size(kRowOutputs)) {
        app::Rdr().audio_output = kRowOutputs[picked];
        audio::SetOutputMode(app::Rdr().audio_output);
        app::Save();
    }
    __imp__sub_00030400(ctx, base);
}
