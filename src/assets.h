// Reads bird sprites and sounds from the player's own Angry Birds install at
// runtime. Nothing is copied to disk; decoded pixels live only in memory.
#pragma once
#include <windows.h>
#include <string>
#include <vector>

struct Sprite {
    HBITMAP bmp = nullptr;   // 32bpp premultiplied BGRA DIB
    HDC dc = nullptr;
    int w = 0, h = 0, pivotX = 0, pivotY = 0;
    bool ok() const { return dc != nullptr; }
};

struct Sound {
    std::vector<char> wav;
    void Play() const;
};

namespace assets {
bool Load(const std::wstring& abDataDir, std::wstring& err);
std::wstring FindAngryBirdsData();   // from birdmod.ini, else default install path
const Sprite& Get(const char* name); // empty Sprite if missing
const Sound& Snd(const wchar_t* fileName);
// Plants vs. Zombies' own seed-packet background, read from the game's main.pak.
bool LoadPacketArt(std::wstring& err);
const Sprite& PacketBackground();
// The "Angry Birds" title lettering, cut from the player's Angry Birds splash screen.
bool LoadTitleArt(const std::wstring& abDataDir, std::wstring& err);
const Sprite& Title(bool small);     // small: main menu badge; else title screen size
}
