#include "app/app.h"
#include "app/menu.h"

// what the runtime asks of the game; strings must be set, hooks may stay null
const app::GameInfo app::kGame = {
    .title = L"Red Dead Revolver",        // window and splash
    .short_title = L"RDR",                // message boxes
    .name = L"RDR",                       // window classes, crash dumps, handover event
    .log = "rdr",                         // log file prefix
    .settings = "settings.toml",          // settings file beside the exe
    .splash = L"splash",                  // optional splash.png or splash.bmp beside the exe
    .missing = L"Put the game in the assets folder: its disc image (.iso), or default.xbe with its folders beside it.",
    .images = {{"default.xbe", "RDR"}},   // each recompiled xbe and its exe, for title launches
    .options = {{"fps_counter", &app::g_fps_counter, nullptr}},  // game settings kept in the settings file
    .load = nullptr,                      // reads more from the settings file's toml::table
    .save = nullptr,                      // writes more into it
    .init = nullptr,                      // after the settings load, before the window and boot
    .loaded = nullptr,                    // image in memory, before its entry point runs
    .window = nullptr,                    // the game window is up
    .message = app::MenuMessage,          // window messages first; true takes one
    .ui = app::DrawUi,                    // ImGui drawing each frame
    .pad = app::MenuPad,                  // changes a polled pad
    .rumble = nullptr,                    // changes a rumble before it is set
};
