// Memory layout of Plants vs. Zombies GOTY, Steam build 1.2.0.1096
// (unpacked image popcapgame1.exe, PE timestamp 0x4D02B058).
// Every offset here was found by scanning the running game, not taken from
// the 1.2.0.1073 tables, which do not match this build.
#pragma once
#include <windows.h>
#include <stdint.h>

namespace pvz {

constexpr uint32_t kPeTimestamp   = 0x4D02B058;
constexpr uintptr_t kAppGlobal    = 0x731C50;   // LawnApp*

// LawnApp
constexpr uint32_t App_Board      = 0x868;      // Board* (null outside a level)

// Board
constexpr uint32_t Board_Zombies  = 0xA8;       // DataArray<Zombie>
constexpr uint32_t Board_SeedBank = 0x15C;      // SeedBank*
constexpr uint32_t Board_Background = 0x5564;   // 0 day,1 night,2 pool,3 fog,4 roof,5 moon
constexpr uint32_t Board_Level    = 0x5568;     // Adventure level number, 1-50 (2-8 = 18)
constexpr uint32_t Board_Sun      = 0x5578;     // int
constexpr uint32_t Board_Clock    = 0x5580;     // game ticks (100/s), stops while paused

// DataArray<T>
struct DataArray { uint8_t* block; uint32_t maxUsed; uint32_t maxSize; uint32_t freeHead;
                   uint32_t size; uint32_t nextKey; const char* name; };

// Zombie (stride 0x168)
constexpr uint32_t Zombie_Stride  = 0x168;
constexpr uint32_t Zombie_Row     = 0x1C;
constexpr uint32_t Zombie_Type    = 0x24;
constexpr uint32_t Zombie_PosX    = 0x2C;       // float
constexpr uint32_t Zombie_PosY    = 0x30;       // float
constexpr uint32_t Zombie_HitRect = 0x8C;       // int x,y,w,h relative to pos
constexpr uint32_t Zombie_HelmType   = 0xC4;
constexpr uint32_t Zombie_BodyHp     = 0xC8;
constexpr uint32_t Zombie_HelmHp     = 0xD0;
constexpr uint32_t Zombie_ShieldType = 0xD8;
constexpr uint32_t Zombie_ShieldHp   = 0xDC;
constexpr uint32_t Zombie_Dead    = 0xEC;       // bool

// SeedBank (a GameObject)
constexpr uint32_t Bank_X = 0x08, Bank_Y = 0x0C, Bank_W = 0x10, Bank_H = 0x14;
constexpr uint32_t Bank_Visible = 0x18;         // bool
constexpr uint32_t Bank_NumPackets = 0x24;      // slots the player fills in the seed chooser
constexpr int SlotFirstX = 69, SlotStepX = 51, SlotY = 8, PacketW = 50, PacketH = 70;  // relative to bank

// Title screen ("Click to start") and main menu widgets; LawnApp holds each while it exists.
// Recognised by their vtables, read from the running game.
constexpr uint32_t App_TitleScreen      = 0x86C;
constexpr uint32_t App_GameSelector     = 0x870;
constexpr uintptr_t kTitleScreenVtbl    = 0x6F99A8, kGameSelectorVtbl = 0x6EEC88;

// SeedChooserScreen ("Choose your plants"); LawnApp holds it while it exists
constexpr uint32_t App_SeedChooser      = 0x874;
constexpr uint32_t Chooser_X            = 0x40, Chooser_Y = 0x44;   // widget position (slides in)
constexpr uint32_t Chooser_StartButton  = 0xA0;   // "Let's Rock!" GameButton*
constexpr uint32_t Chooser_SeedsInBank  = 0xD3C;  // plants picked so far
constexpr uint32_t Button_Disabled      = 0x22;   // bool in GameButton
constexpr uint32_t Button_X = 0x10, Button_Y = 0x14, Button_W = 0x18, Button_H = 0x1C;

// Almanac. It isn't a LawnApp field: the widget manager (LawnApp+0x320) points at it from
// its modal/focus fields while it's open, and it is recognised by its vtable.
constexpr uint32_t App_WidgetManager    = 0x320;
constexpr uint32_t WidgetMgr_Refs[]     = {0xA0, 0xA8, 0xAC};
constexpr uint32_t WidgetMgr_Focus      = 0xA0;   // focused widget
constexpr uint32_t WidgetMgr_BaseModal  = 0xAC;   // lowest open modal dialog (pause menu, Imitater...)
// Last mouse position in game (800x600) coordinates, after the game's own full-screen remap.
// The mod only trusts it once it moves in a straight line with the real cursor.
constexpr uint32_t WidgetMgr_LastMouseX = 0xE0, WidgetMgr_LastMouseY = 0xE4;
constexpr uint32_t Widget_X = 0x40, Widget_Y = 0x44, Widget_W = 0x48, Widget_H = 0x4C;
constexpr uintptr_t kBoardVtbl = 0x6ECC60, kChooserVtbl = 0x6F9180;
constexpr uintptr_t kAlmanacVtbl        = 0x6EB4F8;
constexpr uint32_t Almanac_Close        = 0x194;  // GameButton*, (676,567) 89x26
constexpr uint32_t Almanac_Index        = 0x198;  // GameButton*, (32,567) 164x26
constexpr uint32_t Almanac_ViewPlants   = 0x19C;  // GameButton*, enabled only on the index page
constexpr uint32_t Almanac_ViewZombies  = 0x1A0;
constexpr uint32_t Almanac_ButtonHidden = 0x105;  // bool in the Almanac's buttons: 1 = not drawn, not clickable

// Zombie::DieWithLoot (this = zombie in ecx): removes the zombie (no-loot routine at 0x544FB0,
// which sets Zombie_Dead) and then drops its loot (0x544C20), including the end-of-level award.
constexpr uintptr_t kZombieDieWithLoot = 0x544DA0;
inline void ZombieDieWithLoot(uintptr_t zombie) {
    reinterpret_cast<void(__thiscall*)(uintptr_t)>(kZombieDieWithLoot)(zombie);
}

// SeedChooserScreen's constructor: "if Adventure and the player has finished Adventure before,
// CrazyDavePickSeeds(this)" (0x492690, which locks 3 random plants into the bank). The jle at
// this address skips the call when the player hasn't finished Adventure; jmp skips it always.
constexpr uintptr_t kDavePicksJump = 0x49263B;   // 7E 06 (jle +6) -> EB 06 (jmp +6)

template <class T> inline T rd(uintptr_t a) { return *reinterpret_cast<T*>(a); }
template <class T> inline void wr(uintptr_t a, T v) { *reinterpret_cast<T*>(a) = v; }

inline bool IsSupportedBuild() {
    auto base = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    return nt->FileHeader.TimeDateStamp == kPeTimestamp;
}
inline uintptr_t App()   { return rd<uintptr_t>(kAppGlobal); }
inline uintptr_t Board() { uintptr_t a = App(); return a ? rd<uintptr_t>(a + App_Board) : 0; }

} // namespace pvz
