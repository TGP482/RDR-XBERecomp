#include "app/app.h"
#include "app/game_settings.h"
#include "app/menu.h"
#include "app/window.h"

// what the runtime asks of the game; strings must be set, hooks may stay null
const app::GameInfo app::kGame = {
    .title = L"Red Dead Revolver - XBE Recomp", // window, splash and compiler
    .short_title = L"RDR",                // message boxes
    .name = L"RDR",                       // window classes, crash dumps, handover event
    .log = "rdr",                         // log file prefix
    .settings = "settings.toml",          // settings file beside the exe
    .splash = L"splash",                  // optional splash.png or splash.bmp beside the exe
    .missing = L"Put the game in the assets folder: its disc image (.iso), or default.xbe with its folders beside it.",
    .images = {{"default.xbe", "RDR"}},   // each recompiled xbe and its exe, for title launches
    .options = {},                        // game settings kept in the settings file
    .load = app::LoadRdr,                 // reads more from the settings file's toml::table
    .save = app::SaveRdr,                 // writes more into it
    .init = app::InitInput,               // after the settings load, before the window and boot
    .loaded = nullptr,                    // image in memory, before its entry point runs
    .window = app::WindowUp,              // the game window is up
    .message = app::WindowMessage,        // window messages first; true takes one
    .ui = app::DrawUi,                    // ImGui drawing each frame
    .pad = nullptr,                       // changes a polled pad
    .rumble = nullptr,                    // changes a rumble before it is set
    .releases = {{"default.xbe", "69581bae0af94bec30619f488f290336603bda9b"},   // Europe, config/game.toml
                 {"default.xbe", "6e4f5a20b34afd30eddf34ae258c584f6d49f181"},   // USA, config/game_us.toml
                 {"default.xbe", "7612b620c2b6bdfcd04c4002b24ce561cba056ce"}},  // Japan, config/game_jp.toml
};
