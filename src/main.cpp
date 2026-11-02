#include <Geode/Geode.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include "GeodleLayer.hpp"

using namespace geode::prelude;

// F8 toggles Geodle. While it is open every key-down is routed to it (so Escape/Enter/letters
// never reach the game or a pause menu underneath). Key-ups always pass through so nothing
// gets "stuck" if you open Geodle while holding a jump key.
class $modify(GeodleKeyboardHook, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat) {
        if (key == KEY_F8 && down) {
            if (!repeat) GeodleLayer::toggle();
            return true;
        }
        if (down) {
            if (auto layer = GeodleLayer::get()) {
                layer->onKey(key, repeat);
                return true;
            }
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat);
    }
};
