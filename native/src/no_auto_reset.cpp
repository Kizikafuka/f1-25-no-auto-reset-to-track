// SPDX-License-Identifier: MIT
// No Auto Reset to Track, in-game part. The game resets a car to the track by itself when it stays off the track (the "AI reset to track"
// rule, allowAIAutoReset, is off in Grand Prix and career, but the cars it calls "selected", the human-controlled ones, are reset anyway).
// The reset is one update function, Game_AutoResetToTrack_StartTimer: per car it starts a timer, and when the timer ends it hands the car to
// the AI and teleports it back. This addon makes that function return at once, so no timer ever starts. Off again = the original bytes.
#include <windows.h>

#include <cstdint>
#include <cstring>

#include <swofty/mixin.hpp>

#include "kizi_mem.hpp"

SWOFTY_SETTING(bool, enabled, true);

namespace {

// The start of Game_AutoResetToTrack_StartTimer (push r15 ... sub rsp,0x1d8, the xmm saves, mov [rsp+68h],rdx, mov rdi,rcx, ...), 0x8e
// bytes; the last four are the displacement of a jump inside the function.
const char *kPattern =
    "41 57 41 56 41 55 41 54 56 57 55 53 48 81 EC D8 01 00 00 C5 78 29 BC 24 C0 01 00 00 C5 78 29 B4 24 B0 01 00 00 C5 78 29 AC 24 A0 01 00 00 "
    "C5 78 29 A4 24 90 01 00 00 C5 78 29 9C 24 80 01 00 00 C5 78 29 94 24 70 01 00 00 C5 78 29 8C 24 60 01 00 00 C5 78 29 84 24 50 01 00 00 "
    "C5 F8 29 BC 24 40 01 00 00 C5 F8 29 B4 24 30 01 00 00 48 89 54 24 68 48 89 CF 48 8B 81 B0 00 00 00 8B 48 30 2B 48 28 F7 C1 F8 07 00 00 "
    "0F 84 ?? ?? ?? ??";

uintptr_t g_fn = 0;          // the function, 16-byte aligned
uint8_t g_orig[8] = {};      // its first 8 bytes
bool g_have = false, g_on = false;

// The first 8 bytes are replaced in one atomic store: xor eax,eax; ret (the original bytes 3..7 follow, never executed).
bool set_patched(bool on)
{
    if (!g_have || g_on == on)
        return true;
    uint8_t want[8];
    memcpy(want, g_orig, 8);
    if (on) {
        want[0] = 0x31;
        want[1] = 0xC0;
        want[2] = 0xC3;
    }
    uint8_t cur[8];
    if (!kizi::read(g_fn, cur, 8))
        return false;
    uint64_t expect, repl;
    memcpy(&expect, cur, 8);
    memcpy(&repl, want, 8);
    DWORD old = 0, tmp = 0;
    if (!VirtualProtect(reinterpret_cast<void *>(g_fn), 8, PAGE_EXECUTE_READWRITE, &old))
        return false;
    LONG64 prev = InterlockedCompareExchange64(reinterpret_cast<volatile LONG64 *>(g_fn), static_cast<LONG64>(repl), static_cast<LONG64>(expect));
    VirtualProtect(reinterpret_cast<void *>(g_fn), 8, old, &tmp);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void *>(g_fn), 8);
    if (static_cast<uint64_t>(prev) != expect)
        return false;
    g_on = on;
    swofty::log("[no-auto-reset] auto reset to track is %s", on ? "OFF (the game's reset function returns at once)" : "back on (original code)");
    return true;
}

}  // namespace

SWOFTY_ON_LOAD
{
    g_fn = kizi::scan_code(kPattern, "Game_AutoResetToTrack_StartTimer");
    if (!g_fn) {
        swofty::log("[no-auto-reset] the reset function was not found on this game build: nothing changed");
        return;
    }
    if (g_fn & 15)
        swofty::log("[no-auto-reset] the function is not 16-byte aligned (%p): nothing changed", reinterpret_cast<void *>(g_fn));
    else if (!kizi::read(g_fn, g_orig, 8) || g_orig[0] != 0x41 || g_orig[1] != 0x57)
        swofty::log("[no-auto-reset] unexpected first bytes at the reset function: nothing changed");
    else {
        g_have = true;
        swofty::log("[no-auto-reset] found Game_AutoResetToTrack_StartTimer at %p", reinterpret_cast<void *>(g_fn));
        if (enabled.get() && !set_patched(true))
            swofty::log("[no-auto-reset] could not patch the function");
    }
}

SWOFTY_ON_CHANGE(enabled, bool on)
{
    if (!set_patched(on))
        swofty::log("[no-auto-reset] could not change the function");
}

SWOFTY_ADDON()
