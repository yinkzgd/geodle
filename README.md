# GEODLE - a daily Wordle for Geometry Dash (Geode mod) (This mod was made with Claude AI.)

Press **F8** anywhere (main menu or inside a level) to open Geodle. F8 / Esc / the X button close it.

## Build
Requirements: [Geode SDK + CLI](https://docs.geode-sdk.org/getting-started/), CMake 3.21+, a C++20 compiler
(Visual Studio 2022 on Windows, Xcode on macOS). Target: Geometry Dash 2.2074, Geode 4.x.

    geode sdk install            # once
    geode sdk install-binaries   # once per platform
    cd geodle
    geode build                  # or: cmake -B build && cmake --build build --config RelWithDebInfo

Windows 2.2074 is 32-bit: if you use raw CMake pass `-A win32`.
This produces `yourname.geodle.geode`.

## Install
Copy `yourname.geodle.geode` into `<Geometry Dash>/geode/mods/` (or run `geode package install <file>`), then launch the game.
Before publishing, change `id` / `developer` in `mod.json` (`yourname.geodle`).

## Layout
| File | Purpose |
|---|---|
| `src/main.cpp` | F8 hook on `CCKeyboardDispatcher` (routes keys to Geodle while it is open) |
| `src/GeodleLayer.*` | All UI: home, game grid, keyboard, stats, achievements, help, history, animations |
| `src/Logic.*` | Wordle scoring (duplicate letters), UTC day number, countdown, dates |
| `src/Words.*` | Answer pool (`kAnswerData`), valid-guess list, deterministic no-repeat daily schedule |
| `src/Save.*` | Persistent stats, streaks, history, in-progress game (Geode saved values) |
| `src/Achievements.*` | Achievement definitions + unlock toast |
| `resources/valid_guesses.txt` | Optional extra accepted guesses (one word per line) |

## Extending
* **Add answers:** append to `kAnswerData` in `Words.cpp` (invalid words are dropped + logged). Changing the pool changes the daily schedule, so ship it with an update.
* **Wider guess dictionary:** paste a big 5-letter word list into `resources/valid_guesses.txt`.
* **Add achievements:** add one `add(...)` line in `Achievements.cpp::build()`; the page paginates automatically (12 per page).

## Design notes
* Daily word = f(UTC day number), so every player gets the same word on the same UTC day; rollover is 00:00 UTC (the countdown shows this).
* Answers come from a seeded Fisher-Yates permutation of the sorted pool per cycle (splitmix64, platform independent) so nothing repeats until the pool is exhausted.
* Streak = consecutive daily wins; it resets on a loss or on any missed day.
* Every guess is saved the moment it is submitted, so quitting mid-animation never loses progress.
* The brief lists STEREO, which is six letters, so it can't be an answer; STEREO MADNESS is represented by other words.

## Verification status (please read)
This project was written without access to a compiler, the Geode SDK or Geometry Dash, so **it has not been built or run in-game**.
What *was* verified here: the pure logic (scoring incl. duplicates, date math, countdown) compiles and passes spot tests, and all built-in
words are exactly five letters with no duplicates. Everything touching Geode/cocos2d (UI, hooks, saving, animations) is unverified: expect
to fix a few compile errors on first build. Likely spots:
* `dispatchKeyboardMSG` signature in `main.cpp` (GD 2.2074 has 3 params; 2.2081+/Geode 5 adds a timestamp).
* `geode::cocos::getMousePos()` (hover effect), `SceneManager::keepAcrossScenes`, `FMODAudioEngine::playEffect` default args.
* Vanilla sprite frame names used for achievement icons (`GJ_starsIcon_001.png` etc.); missing ones automatically fall back to a text glyph.

Checklist to run through in-game: F8 open/close on menu and in a level; Esc closes; Enter/Backspace/letters; invalid word shake;
duplicate-letter colours; win/lose screens; stats + streak after a win; restart game and confirm progress persists;
change system date by +1 day to confirm new puzzle/streak behaviour; resize window to check scaling.
