#include "assets.h"
#include <map>
#include <algorithm>
#include <fstream>
#include <mmsystem.h>
#include <wincodec.h>
#include <shlwapi.h>

namespace {

std::map<std::string, Sprite> g_sprites;
std::map<std::wstring, Sound> g_sounds;
Sprite g_none;
Sound g_silent;

bool ReadFile(const std::wstring& path, std::vector<uint8_t>& out) {
    std::ifstream f(path.c_str(), std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), {});
    return true;
}

uint16_t be16(const uint8_t* p) { return uint16_t(p[0] << 8 | p[1]); }

struct AtlasEntry { std::string image; int x, y, w, h, px, py; };

// KA3D/SPRT atlas: big-endian. [u16 imageCount] { [u16 len][name] [u16 n] { [u16 len][name] x y w h px py } }
bool ParseAtlas(const std::vector<uint8_t>& d, std::map<std::string, AtlasEntry>& out) {
    if (d.size() < 18 || memcmp(d.data(), "KA3D", 4) || memcmp(d.data() + 8, "SPRT", 4)) return false;
    size_t p = 16;
    auto need = [&](size_t n) { return p + n <= d.size(); };
    if (!need(2)) return false;
    int images = be16(&d[p]); p += 2;
    for (int i = 0; i < images; ++i) {
        if (!need(2)) return false;
        int l = be16(&d[p]); p += 2;
        if (!need(l + 2)) return false;
        std::string img((const char*)&d[p], l); p += l;
        int n = be16(&d[p]); p += 2;
        for (int j = 0; j < n; ++j) {
            if (!need(2)) return false;
            l = be16(&d[p]); p += 2;
            if (!need(l + 12)) return false;
            std::string name((const char*)&d[p], l); p += l;
            const uint8_t* q = &d[p]; p += 12;
            out[name] = {img, be16(q), be16(q + 2), be16(q + 4), be16(q + 6), be16(q + 8), be16(q + 10)};
        }
    }
    return true;
}

// PVR v3, uncompressed RGBA4444 or RGBA8888.
struct Pvr { int w = 0, h = 0, bpp = 0; const uint8_t* px = nullptr; };
bool ParsePvr(const std::vector<uint8_t>& d, Pvr& out) {
    if (d.size() < 52 || memcmp(d.data(), "PVR\x03", 4)) return false;
    uint64_t fmt; memcpy(&fmt, &d[8], 8);
    uint32_t h, w, meta; memcpy(&h, &d[24], 4); memcpy(&w, &d[28], 4); memcpy(&meta, &d[48], 4);
    const uint64_t RGBA4444 = 0x0404040461626772ull, RGBA8888 = 0x0808080861626772ull;
    out.bpp = fmt == RGBA4444 ? 2 : fmt == RGBA8888 ? 4 : 0;
    if (!out.bpp) return false;
    out.w = (int)w; out.h = (int)h; out.px = d.data() + 52 + meta;
    return 52 + meta + size_t(w) * h * out.bpp <= d.size();
}

Sprite MakeSprite(const Pvr& img, const AtlasEntry& e) {
    Sprite s;
    if (e.x + e.w > img.w || e.y + e.h > img.h) return s;
    BITMAPINFO bi{}; bi.bmiHeader = {sizeof(BITMAPINFOHEADER), e.w, -e.h, 1, 32, BI_RGB};
    void* bits = nullptr;
    s.bmp = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!s.bmp) return s;
    auto dst = static_cast<uint8_t*>(bits);
    for (int r = 0; r < e.h; ++r)
        for (int c = 0; c < e.w; ++c) {
            size_t i = (size_t(e.y + r) * img.w + e.x + c) * img.bpp;
            int R, G, B, A;
            if (img.bpp == 2) {
                uint16_t v = uint16_t(img.px[i] | img.px[i + 1] << 8);
                R = (v >> 12 & 15) * 17; G = (v >> 8 & 15) * 17; B = (v >> 4 & 15) * 17; A = (v & 15) * 17;
            } else { R = img.px[i]; G = img.px[i + 1]; B = img.px[i + 2]; A = img.px[i + 3]; }
            uint8_t* o = dst + (size_t(r) * e.w + c) * 4;
            o[0] = uint8_t(B * A / 255); o[1] = uint8_t(G * A / 255); o[2] = uint8_t(R * A / 255); o[3] = uint8_t(A);
        }
    s.dc = CreateCompatibleDC(nullptr);
    SelectObject(s.dc, s.bmp);
    s.w = e.w; s.h = e.h; s.pivotX = e.px; s.pivotY = e.py;
    return s;
}

std::wstring Widen(const std::string& s) { return std::wstring(s.begin(), s.end()); }

} // namespace

void Sound::Play() const {
    if (!wav.empty()) PlaySoundA(wav.data(), nullptr, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}

namespace assets {

const char* kSprites[] = {
    "BIRD_RED", "BIRD_RED_FLYING", "BIRD_RED_COLLISION",
    "BIRD_YELLOW", "BIRD_YELLOW_FLYING", "BIRD_YELLOW_SPECIAL",
    "BIRD_GREY", "BIRD_GREY_FLYING", "BIRD_GREY_3",
    "BIRD_BLUE", "BIRD_BLUE_COLLISION",
    "BIRD_BOOMERANG", "BIRD_BOOMERANG_SPECIAL",
    "BIRD_BIG_BROTHER", "BIRD_BIG_BROTHER_YELL",
    "BIRD_PUFFER_1", "BIRD_PUFFER_2",
    "BIRD_PINK", "BIRD_PINK_RELEASE",
};
const wchar_t* kSounds[] = {
    L"slingshot streched.wav",
    L"bird 01 select.wav", L"bird 02 select.wav", L"bird 03 select.wav", L"bird 04 select.wav", L"bird 05 select.wav",
    L"bird 01 flying.wav", L"bird 02 flying.wav", L"bird 03 flying.wav", L"bird 04 flying.wav", L"bird 05 flying.wav",
    L"bird 01 collision a1.wav", L"bird destroyed.wav", L"big_bird.wav",
    L"special boost.wav", L"special group.wav", L"special egg.wav", L"tnt box explodes.wav",
    L"Globe_Bird_Special_Activation_1.wav", L"Globe_Bird_Special_Activation_2.wav", L"Globe_Bird_Special_Activation_3.wav",
    // pig structures
    L"light damage a1.wav", L"light destroyed a1.wav", L"ice light collision a1.wav",
    L"wood damage a1.wav", L"wood destroyed a1.wav", L"wood collision a1.wav",
    L"rock damage a1.wav", L"rock destroyed a1.wav", L"rock collision a1.wav",
    L"piglette damage a1.wav", L"piglette destroyed.wav", L"piglette oink a1.wav",
};

// Pig structure art: BLOCK_<LIGHT|WOOD|ROCK>_<damage 1-4>_<shape 2-6> and the small pig.
static std::vector<std::string> StructureSprites() {
    std::vector<std::string> v;
    for (const char* m : {"LIGHT", "WOOD", "ROCK"})
        for (int d = 1; d <= 4; ++d)
            for (int sh = 2; sh <= 6; ++sh) { char n[40]; wsprintfA(n, "BLOCK_%s_%d_%d", m, d, sh); v.push_back(n); }
    for (const char* p : {"PIG_BASIC_SMALL_01", "PIG_BASIC_SMALL_02", "PIG_BASIC_SMALL_03"}) v.push_back(p);
    return v;
}
bool DecodePng(const std::vector<uint8_t>& png, std::vector<uint8_t>& px, int& w, int& h);

// A value from birdmod.ini next to birdmod.dll (the launcher writes it from its arguments).
static std::wstring IniValue(const wchar_t* key) {
    wchar_t ini[MAX_PATH]; HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCWSTR)&IniValue, &self);
    GetModuleFileNameW(self, ini, MAX_PATH);
    if (wchar_t* slash = wcsrchr(ini, L'\\')) wcscpy(slash + 1, L"birdmod.ini");
    wchar_t dir[MAX_PATH] = L"";
    GetPrivateProfileStringW(L"birdmod", key, L"", dir, MAX_PATH, ini);
    std::wstring v = dir;
    for (auto& c : v) if (c == L'/') c = L'\\';
    while (!v.empty() && v.back() == L'\\') v.pop_back();
    return v;
}

std::wstring FindAngryBirdsData() {
    std::wstring root = IniValue(L"AngryBirdsFolder");
    // Accept the data folder itself as well as the install folder.
    if (!root.empty() && GetFileAttributesW((root + L"\\images").c_str()) != INVALID_FILE_ATTRIBUTES &&
        GetFileAttributesW((root + L"\\data").c_str()) == INVALID_FILE_ATTRIBUTES) return root;
    if (root.empty()) {
        wchar_t pf[MAX_PATH] = L"";
        if (!GetEnvironmentVariableW(L"ProgramFiles(x86)", pf, MAX_PATH)) GetEnvironmentVariableW(L"ProgramFiles", pf, MAX_PATH);
        root = std::wstring(pf) + L"\\Rovio Entertainment Ltd\\Angry Birds";
    }
    return root + L"\\data";
}

bool Load(const std::wstring& data, std::wstring& err) {
    std::map<std::string, AtlasEntry> atlas;
    for (const wchar_t* f : {L"INGAME_BIRDS_1.dat", L"INGAME_BIRDS_2.dat", L"INGAME_TRANSPARENCY.dat",
                             L"INGAME_BLOCKS_1.dat", L"INGAME_EXPLOSIONS.dat", L"INGAME_NEW_PIGS_ELEMENTS_2.dat"}) {
        std::vector<uint8_t> d;
        if (ReadFile(data + L"\\images\\base\\" + f, d)) ParseAtlas(d, atlas);
    }
    if (!atlas.count("BIRD_RED")) { err = L"Could not read the bird atlas in " + data + L"\\images\\base"; return false; }

    std::map<std::string, std::pair<std::vector<uint8_t>, Pvr>> images;
    std::vector<std::string> names(std::begin(kSprites), std::end(kSprites));
    for (auto& n : StructureSprites()) names.push_back(n);
    for (const std::string& name : names) {
        auto it = atlas.find(name);
        if (it == atlas.end()) continue;
        const std::string& img = it->second.image;
        bool forBlocks = name.rfind("BLOCK_", 0) == 0 || name.rfind("PIG_", 0) == 0;
        const std::string key = forBlocks ? img + "#blocks" : img;   // block sheets get their own PNG fallback
        if (!images.count(key)) {
            auto& slot = images[key];
            std::wstring path = data + L"\\images\\base\\" + Widen(img);
            bool ok = ReadFile(path, slot.first) && ParsePvr(slot.first, slot.second);
            // Block sheets named as PNGs (the ice blocks): decode the PNG into RGBA8888.
            // Bird sheets keep the old behaviour (skipped; flying variants fall back to idle).
            if (!ok && forBlocks && img.size() > 4 && !_stricmp(img.c_str() + img.size() - 4, ".png")) {
                std::vector<uint8_t> png; int w = 0, h = 0;
                if (ReadFile(path, png) && DecodePng(png, slot.first, w, h)) {
                    for (size_t i = 0; i + 3 < slot.first.size(); i += 4) std::swap(slot.first[i], slot.first[i + 2]);   // BGRA -> RGBA
                    slot.second.w = w; slot.second.h = h; slot.second.bpp = 4; slot.second.px = slot.first.data();
                    ok = true;
                }
            }
            if (!ok) slot.second.px = nullptr;
        }
        auto& im = images[key];
        if (im.second.px) g_sprites[name] = MakeSprite(im.second, it->second);
    }
    for (const char* need : {"BIRD_RED", "BIRD_YELLOW", "BIRD_GREY", "BIRD_BLUE", "BIRD_BOOMERANG",
                             "BIRD_BIG_BROTHER", "BIRD_PUFFER_1", "BIRD_PINK"})
        if (!g_sprites[need].ok()) { err = L"Missing sprite " + Widen(need); return false; }

    for (const wchar_t* s : kSounds) {
        std::vector<uint8_t> d;
        if (ReadFile(data + L"\\audio\\sfx\\" + s, d)) g_sounds[s].wav.assign(d.begin(), d.end());
    }
    return true;
}

const Sprite& Get(const char* name) {
    auto it = g_sprites.find(name);
    return it != g_sprites.end() && it->second.ok() ? it->second : g_none;
}
// ---- PvZ main.pak: every byte XOR 0xF7. Header (8 bytes), then entries
// [u8 flag][u8 nameLen][name][u32 size][u64 time] until flag 0x80, then file data in order.
Sprite g_packetBg;

static bool ReadPakFile(const std::wstring& pak, const char* want, std::vector<uint8_t>& out) {
    std::vector<uint8_t> d;
    if (!ReadFile(pak, d) || d.size() < 16) return false;
    for (auto& b : d) b ^= 0xF7;
    if (d[0] != 0xC0 || d[1] != 0x4A || d[2] != 0xC0 || d[3] != 0xBA) return false;
    size_t p = 8; uint64_t dataOff = 0, found = UINT64_MAX, foundSize = 0;
    while (p < d.size() && d[p] != 0x80) {
        ++p; uint8_t n = d[p++];
        if (p + n + 12 > d.size()) return false;
        std::string name((const char*)&d[p], n); p += n;
        uint32_t size; memcpy(&size, &d[p], 4); p += 12;
        if (!_stricmp(name.c_str(), want)) { found = dataOff; foundSize = size; }
        dataOff += size;
    }
    if (found == UINT64_MAX) return false;
    size_t start = p + 1 + size_t(found);
    if (start + foundSize > d.size()) return false;
    out.assign(d.begin() + start, d.begin() + start + foundSize);
    return true;
}

// Decodes a PNG with WIC and cuts a w x h cell from (x, y) into a premultiplied DIB sprite.
static Sprite DecodePngCell(const std::vector<uint8_t>& png, int x, int y, int w, int h) {
    Sprite s;
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IWICImagingFactory* f = nullptr; IWICBitmapDecoder* dec = nullptr; IWICBitmapFrameDecode* fr = nullptr;
    IWICFormatConverter* cv = nullptr;
    IStream* st = SHCreateMemStream(png.data(), UINT(png.size()));
    if (st && SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&f))) &&
        SUCCEEDED(f->CreateDecoderFromStream(st, nullptr, WICDecodeMetadataCacheOnLoad, &dec)) &&
        SUCCEEDED(dec->GetFrame(0, &fr)) && SUCCEEDED(f->CreateFormatConverter(&cv)) &&
        SUCCEEDED(cv->Initialize(fr, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom))) {
        BITMAPINFO bi{}; bi.bmiHeader = {sizeof(BITMAPINFOHEADER), w, -h, 1, 32, BI_RGB};
        void* bits = nullptr;
        s.bmp = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        WICRect r{x, y, w, h};
        if (s.bmp && SUCCEEDED(cv->CopyPixels(&r, UINT(w * 4), UINT(w * h * 4), static_cast<BYTE*>(bits)))) {
            s.dc = CreateCompatibleDC(nullptr); SelectObject(s.dc, s.bmp);
            s.w = w; s.h = h;
        }
    }
    for (IUnknown* u : {(IUnknown*)cv, (IUnknown*)fr, (IUnknown*)dec, (IUnknown*)f, (IUnknown*)st}) if (u) u->Release();
    return s;
}

// ---- The "Angry Birds" title, cut from the player's own Angry Birds splash screen.
// The letters are white with a dark outline, painted over sky and clouds: everything reachable
// from the crop's edge without crossing the outline is background, and so is sky showing
// inside the letters (A, R, B, D).
Sprite g_title[2];   // 0: title screen size, 1: main menu badge size

bool DecodePng(const std::vector<uint8_t>& png, std::vector<uint8_t>& px, int& w, int& h) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IWICImagingFactory* f = nullptr; IWICBitmapDecoder* dec = nullptr; IWICBitmapFrameDecode* fr = nullptr;
    IWICFormatConverter* cv = nullptr;
    IStream* st = SHCreateMemStream(png.data(), UINT(png.size()));
    bool ok = false; UINT uw = 0, uh = 0;
    if (st && SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&f))) &&
        SUCCEEDED(f->CreateDecoderFromStream(st, nullptr, WICDecodeMetadataCacheOnLoad, &dec)) &&
        SUCCEEDED(dec->GetFrame(0, &fr)) && SUCCEEDED(fr->GetSize(&uw, &uh)) && SUCCEEDED(f->CreateFormatConverter(&cv)) &&
        SUCCEEDED(cv->Initialize(fr, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom))) {
        px.resize(size_t(uw) * uh * 4);
        ok = SUCCEEDED(cv->CopyPixels(nullptr, uw * 4, UINT(px.size()), px.data()));
        w = int(uw); h = int(uh);
    }
    for (IUnknown* u : {(IUnknown*)cv, (IUnknown*)fr, (IUnknown*)dec, (IUnknown*)f, (IUnknown*)st}) if (u) u->Release();
    return ok;
}

// Box-filter downscale of a straight-alpha BGRA image into a premultiplied DIB sprite.
static Sprite ScaledSprite(const std::vector<uint8_t>& src, int sw, int sh, int dw) {
    Sprite s;
    int dh = std::max(1, sh * dw / sw);
    BITMAPINFO bi{}; bi.bmiHeader = {sizeof(BITMAPINFOHEADER), dw, -dh, 1, 32, BI_RGB};
    void* bits = nullptr;
    s.bmp = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!s.bmp) return s;
    auto dst = static_cast<uint8_t*>(bits);
    for (int y = 0; y < dh; ++y)
        for (int x = 0; x < dw; ++x) {
            int x0 = x * sw / dw, x1 = std::max(x0 + 1, (x + 1) * sw / dw);
            int y0 = y * sh / dh, y1 = std::max(y0 + 1, (y + 1) * sh / dh);
            double b = 0, g = 0, r = 0, a = 0; int n = 0;
            for (int yy = y0; yy < y1; ++yy)
                for (int xx = x0; xx < x1; ++xx) {
                    const uint8_t* p = &src[(size_t(yy) * sw + xx) * 4];
                    double al = p[3] / 255.0;
                    b += p[0] * al; g += p[1] * al; r += p[2] * al; a += p[3]; ++n;
                }
            uint8_t* o = dst + (size_t(y) * dw + x) * 4;
            o[0] = uint8_t(b / n + .5); o[1] = uint8_t(g / n + .5); o[2] = uint8_t(r / n + .5); o[3] = uint8_t(a / n + .5);
        }
    s.dc = CreateCompatibleDC(nullptr);
    SelectObject(s.dc, s.bmp);
    s.w = dw; s.h = dh;
    return s;
}

bool LoadTitleArt(const std::wstring& data, std::wstring& err) {
    const std::wstring dir = data + L"\\images\\1024x768_splash\\";
    std::vector<uint8_t> d;
    std::map<std::string, AtlasEntry> atlas;
    if (!ReadFile(dir + L"SPLASHES_SHEET_1.dat", d) || !ParseAtlas(d, atlas) || !atlas.count("SPLASH_ANGRY_BIRDS")) {
        err = L"No SPLASH_ANGRY_BIRDS in " + dir; return false;
    }
    const AtlasEntry& e = atlas["SPLASH_ANGRY_BIRDS"];
    std::vector<uint8_t> png, img; int iw = 0, ih = 0;
    if (!ReadFile(dir + Widen(e.image), png) || !DecodePng(png, img, iw, ih)) { err = L"Could not decode " + Widen(e.image); return false; }
    if (e.w != 1024 || e.h != 768 || e.x + e.w > iw || e.y + e.h > ih) { err = L"Unexpected splash layout"; return false; }
    // Where the title sits on the 1024x768 splash.
    const int cx0 = e.x + 185, cy0 = e.y + 45, cw = 660, ch = 150;
    std::vector<uint8_t> cut(size_t(cw) * ch * 4);
    for (int y = 0; y < ch; ++y) memcpy(&cut[size_t(y) * cw * 4], &img[(size_t(cy0 + y) * iw + cx0) * 4], size_t(cw) * 4);
    auto dark = [&](int i) { const uint8_t* p = &cut[size_t(i) * 4]; return std::max({p[0], p[1], p[2]}) < 90; };
    std::vector<uint8_t> bg(size_t(cw) * ch, 0);
    std::vector<int> stack;
    for (int x = 0; x < cw; ++x) { stack.push_back(x); stack.push_back((ch - 1) * cw + x); }
    for (int y = 0; y < ch; ++y) { stack.push_back(y * cw); stack.push_back(y * cw + cw - 1); }
    while (!stack.empty()) {
        int i = stack.back(); stack.pop_back();
        if (bg[i] || dark(i)) continue;
        bg[i] = 1;
        int x = i % cw, y = i / cw;
        if (x > 0) stack.push_back(i - 1);
        if (x < cw - 1) stack.push_back(i + 1);
        if (y > 0) stack.push_back(i - cw);
        if (y < ch - 1) stack.push_back(i + cw);
    }
    int kept = 0;
    for (int i = 0; i < cw * ch; ++i) {
        uint8_t* p = &cut[size_t(i) * 4];
        bool sky = p[0] > p[2] + 40 && p[0] > p[1] + 10;   // blue showing through a letter
        if (bg[i] || sky) p[0] = p[1] = p[2] = p[3] = 0; else ++kept;
    }
    if (kept < cw * ch / 10) { err = L"The Angry Birds title could not be cut out"; return false; }
    g_title[0] = ScaledSprite(cut, cw, ch, 330);
    g_title[1] = ScaledSprite(cut, cw, ch, 190);
    return g_title[0].ok() && g_title[1].ok();
}
const Sprite& Title(bool small) { return g_title[small ? 1 : 0]; }

bool LoadPacketArt(std::wstring& err) {
    // The game runs from ProgramData but reads main.pak from its Steam folder (its working directory).
    wchar_t cwd[MAX_PATH]; GetCurrentDirectoryW(MAX_PATH, cwd);
    std::wstring pf; { wchar_t b[MAX_PATH] = L""; GetEnvironmentVariableW(L"ProgramFiles(x86)", b, MAX_PATH); pf = b; }
    std::vector<uint8_t> png;
    std::wstring pvzDir = IniValue(L"PvzFolder");
    for (const std::wstring& pak : {pvzDir.empty() ? std::wstring(L"<none>") : pvzDir + L"\\main.pak", std::wstring(cwd) + L"\\main.pak", pf + L"\\Steam\\steamapps\\common\\Plants Vs Zombies\\main.pak"})
        if (ReadPakFile(pak, "images\\seeds.png", png)) break;
    if (png.empty()) { err = L"Could not read images\\seeds.png from PvZ's main.pak"; return false; }
    g_packetBg = DecodePngCell(png, 0, 0, 50, 70);   // first cell: the ordinary plant packet
    if (!g_packetBg.ok()) { err = L"Could not decode the seed packet image"; return false; }
    return true;
}
const Sprite& PacketBackground() { return g_packetBg; }

const Sound& Snd(const wchar_t* f) {
    auto it = g_sounds.find(f);
    return it != g_sounds.end() ? it->second : g_silent;
}

} // namespace assets
