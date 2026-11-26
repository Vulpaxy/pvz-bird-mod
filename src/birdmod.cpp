// Angry Birds seed packets + slingshot for Plants vs. Zombies GOTY (Steam 1.2.0.1096).
// Birds are picked on the "Choose your plants" screen and take seed-bank slots.
// Injected into popcapgame1.exe by birdmod_launcher.exe.
//
// Draws through a hook on IDirectDrawSurface7::Blt/Flip (the game presents its
// 800x600 back buffer that way). Input comes from a subclassed WndProc. Game
// effects come from writing sun and zombie health in place; the game removes
// zombies whose health reaches 0 on its own.
#include <windows.h>
#include <ddraw.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include <cstdarg>
#include <map>
#include "pvz.h"
#include "assets.h"

namespace {

// ---------- birds ----------
// Sprites named here are loaded from the player's Angry Birds install (see assets.cpp).
enum BirdKind { Red, Chuck, Bomb, Blues, Hal, Terence, Bubbles, Stella, kBirdCount };
enum Effect { Impact, DashRight, Explode, Split3, Boomerang, ColumnCrush, PuffPush, BubbleTrap };
struct BirdDef {
    const char* name; const char* idle; const char* flying; const char* special;
    int cost; int rechargeTicks; Effect effect; int damage;
    const wchar_t* selectSnd; const wchar_t* flySnd; const wchar_t* landSnd;
};
const BirdDef kBirds[kBirdCount] = {
    {"Red",     "BIRD_RED",         "BIRD_RED_FLYING",    "BIRD_RED_COLLISION",    100,  750, Impact,      600,
     L"bird 01 select.wav", L"bird 01 flying.wav", L"bird 01 collision a1.wav"},
    {"Chuck",   "BIRD_YELLOW",      "BIRD_YELLOW_FLYING", "BIRD_YELLOW_SPECIAL",   150,  750, DashRight,   300,
     L"bird 03 select.wav", L"bird 03 flying.wav", L"special boost.wav"},
    {"Bomb",    "BIRD_GREY",        "BIRD_GREY_FLYING",   "BIRD_GREY_3",           175, 2500, Explode,    1800,
     L"bird 04 select.wav", L"bird 04 flying.wav", L"tnt box explodes.wav"},
    {"Blues",   "BIRD_BLUE",        "BIRD_BLUE",          "BIRD_BLUE_COLLISION",    75,  750, Split3,      250,
     L"bird 02 select.wav", L"bird 02 flying.wav", L"special group.wav"},
    {"Hal",     "BIRD_BOOMERANG",   "BIRD_BOOMERANG",     "BIRD_BOOMERANG_SPECIAL",125,  750, Boomerang,   300,
     L"bird 05 select.wav", L"bird 05 flying.wav", L"special boost.wav"},
    {"Terence", "BIRD_BIG_BROTHER", "BIRD_BIG_BROTHER",   "BIRD_BIG_BROTHER_YELL", 500, 5000, ColumnCrush, 3000,
     L"big_bird.wav",       L"bird 01 flying.wav", L"bird destroyed.wav"},
    {"Bubbles", "BIRD_PUFFER_1",    "BIRD_PUFFER_1",      "BIRD_PUFFER_2",         100, 1500, PuffPush,    100,
     L"Globe_Bird_Special_Activation_1.wav", L"bird 02 flying.wav", L"Globe_Bird_Special_Activation_2.wav"},
    {"Stella",  "BIRD_PINK",        "BIRD_PINK",          "BIRD_PINK_RELEASE",     125, 2000, BubbleTrap,    0,
     L"Globe_Bird_Special_Activation_3.wav", L"bird 05 flying.wav", L"special egg.wav"},
};
constexpr int kImpactRadius = 60, kSplitRadius = 50, kDashSpeed = 9, kCell = 80;
constexpr int kPushDistance = 120, kTrapTicks = 500;

// ---------- layout (game coordinates, 800x600) ----------
constexpr float kSlingX = 42, kSlingY = 330, kMaxPull = 90;
constexpr float kLawnLeft = 80, kLawnRight = 800, kLawnTop = 80;

struct Shot { BirdKind kind; float x0, y0, x1, y1; int t, dur; bool primary; };
struct Dash { BirdKind kind; float x, y, endX; int row, dir; std::vector<uintptr_t> hit; };
struct Fx { enum Kind { Ring, Blast, Sprite, Number, Bubble } kind; float x, y; int t, life, value; BirdKind bird; };
struct Trap { uintptr_t zombie; float x; int ticks; };

struct State {
    uintptr_t board = 0;
    int lastClock = -1, idleFrames = 0;
    bool running = false;
    int recharge[kBirdCount] = {};          // ticks left
    int loaded = -1;                        // bird in the slingshot
    bool loadedFromBelt = false;            // that bird came off the conveyor belt (free)
    bool conveyor = false;                  // this level hands out packets on a belt
    int beltCount = -1;                     // packets on the belt last frame
    bool dragging = false;
    float pullX = kSlingX, pullY = kSlingY;
    std::vector<Shot> shots;
    std::vector<Dash> dashes;
    std::vector<Fx> fx;
    std::vector<Trap> traps;
    std::vector<int> order;                 // seed bank slots in pick order: -1 plant, else bird
    std::map<int, int> plantIdx;            // chooser: plant seed type -> game bank index
    int origPackets = -1;                   // seed bank slots before birds took any
    bool chooserSeen = false, levelSetup = false, levelChecked = false;
} S;

void Log(const char* fmt, ...);
void RehookWindowMaybe();
void SyncChooser();
uintptr_t Bank();
int BirdCount();

// ---------- lawn geometry ----------
int RowCount(uintptr_t b) { int bg = pvz::rd<int>(b + pvz::Board_Background); return (bg == 2 || bg == 3) ? 6 : 5; }
float RowHeight(uintptr_t b) { return RowCount(b) == 6 ? 85.f : 100.f; }
int RowAt(uintptr_t b, float y) {
    int r = int((y - kLawnTop) / RowHeight(b));
    return std::clamp(r, 0, RowCount(b) - 1);
}
float RowCenterY(uintptr_t b, int row) { return kLawnTop + (row + 0.5f) * RowHeight(b); }

// ---------- zombies ----------
template <class F> void ForEachZombie(uintptr_t b, F f) {
    auto& arr = *reinterpret_cast<pvz::DataArray*>(b + pvz::Board_Zombies);
    for (uint32_t i = 0; i < arr.maxUsed; ++i) {
        uintptr_t z = uintptr_t(arr.block) + i * pvz::Zombie_Stride;
        if (pvz::rd<uint8_t>(z + pvz::Zombie_Dead)) continue;
        if (pvz::rd<int>(z + pvz::Zombie_BodyHp) <= 0) continue;
        int* rect = reinterpret_cast<int*>(z + pvz::Zombie_HitRect);
        float cx = pvz::rd<float>(z + pvz::Zombie_PosX) + rect[0] + rect[2] / 2.f;
        f(z, pvz::rd<int>(z + pvz::Zombie_Row), cx);
    }
}

void Damage(uintptr_t z, int amount, BirdKind by) {
    int before = pvz::rd<int>(z + pvz::Zombie_BodyHp);
    int total = amount;
    auto soak = [&](uint32_t typeOff, uint32_t hpOff) {
        if (!pvz::rd<int>(z + typeOff)) return;
        int hp = pvz::rd<int>(z + hpOff), take = std::min(hp, amount);
        pvz::wr<int>(z + hpOff, hp - take); amount -= take;
    };
    soak(pvz::Zombie_ShieldType, pvz::Zombie_ShieldHp);
    soak(pvz::Zombie_HelmType, pvz::Zombie_HelmHp);
    int after = std::max(0, before - amount);
    pvz::wr<int>(z + pvz::Zombie_BodyHp, after);
    // The game only checks for death inside its own damage routine, so a zombie left at 0
    // health keeps walking until something else hits it. Kill it through the game's own
    // "die with loot" routine: it removes the zombie and drops its loot, which is also what
    // spawns the money bag / trophy when it's the last zombie of the level.
    if (after == 0) pvz::ZombieDieWithLoot(z);
    int row = pvz::rd<int>(z + pvz::Zombie_Row);
    float x = pvz::rd<float>(z + pvz::Zombie_PosX), y = pvz::rd<float>(z + pvz::Zombie_PosY);
    Log("hit: %s -> zombie type %d row %d x %d, damage %d, body %d -> %d",
        kBirds[by].name, pvz::rd<int>(z + pvz::Zombie_Type), row, int(x), total, before, after);
    S.fx.push_back({Fx::Number, x + 60, y + 10, 0, 80, total, by});
}

// Hits every zombie whose row is within rowSpan of `row` and whose centre is within dx of x.
int HitArea(int row, int rowSpan, float x, float dx, int damage, BirdKind by) {
    int n = 0;
    ForEachZombie(S.board, [&](uintptr_t z, int r, float cx) {
        if (std::abs(r - row) <= rowSpan && std::fabs(cx - x) <= dx) { Damage(z, damage, by); ++n; }
    });
    return n;
}

// ---------- game logic ----------
void Land(const Shot& s) {
    const BirdDef& d = kBirds[s.kind];
    uintptr_t b = S.board;
    int row = RowAt(b, s.y1);
    float cy = RowCenterY(b, row);
    if (s.primary) assets::Snd(d.landSnd).Play();
    S.fx.push_back({Fx::Sprite, s.x1, cy, 0, 40, 0, s.kind});
    int hits = 0;
    switch (d.effect) {
    case Impact:
        hits = HitArea(row, 0, s.x1, kImpactRadius, d.damage, s.kind);
        S.fx.push_back({Fx::Ring, s.x1, cy, 0, 25, kImpactRadius});
        break;
    case Split3:
        hits = HitArea(row, 0, s.x1, kSplitRadius, d.damage, s.kind);
        S.fx.push_back({Fx::Ring, s.x1, cy, 0, 20, kSplitRadius});
        break;
    case DashRight:
        S.dashes.push_back({s.kind, s.x1, cy, 850, row, +1, {}});
        break;
    case Boomerang:
        S.dashes.push_back({s.kind, s.x1, cy, kLawnLeft - 30, row, -1, {}});
        break;
    case Explode:
        hits = HitArea(row, 1, s.x1, kCell * 1.5f, d.damage, s.kind);
        S.fx.push_back({Fx::Blast, s.x1, cy, 0, 40, int(kCell * 1.6f)});
        break;
    case ColumnCrush:
        hits = HitArea(row, 9, s.x1, kCell / 2.f + 5, d.damage, s.kind);
        for (int r = 0; r < RowCount(b); ++r) S.fx.push_back({Fx::Ring, s.x1, RowCenterY(b, r), 0, 30, 45});
        break;
    case PuffPush:
        ForEachZombie(b, [&](uintptr_t z, int r, float cx) {
            if (std::abs(r - row) <= 1 && std::fabs(cx - s.x1) <= kCell * 1.5f) {
                Damage(z, d.damage, s.kind); ++hits;
                float& px = *reinterpret_cast<float*>(z + pvz::Zombie_PosX);
                px = std::min(px + kPushDistance, 780.f);
            }
        });
        S.fx.push_back({Fx::Ring, s.x1, cy, 0, 30, int(kCell * 1.6f)});
        break;
    case BubbleTrap:
        ForEachZombie(b, [&](uintptr_t z, int r, float cx) {
            if (std::abs(r - row) <= 1 && std::fabs(cx - s.x1) <= kCell * 1.5f) {
                S.traps.push_back({z, pvz::rd<float>(z + pvz::Zombie_PosX), kTrapTicks}); ++hits;
                Log("trap: Stella -> zombie row %d x %d", r, int(cx));
            }
        });
        break;
    }
    Log("land: %s at x %d row %d, %d zombie(s) affected", d.name, int(s.x1), row, hits);
}

void Tick() {
    for (int& r : S.recharge) if (r > 0) --r;
    for (auto it = S.shots.begin(); it != S.shots.end();) {
        if (++it->t >= it->dur) { Shot s = *it; it = S.shots.erase(it); Land(s); } else ++it;
    }
    for (auto it = S.dashes.begin(); it != S.dashes.end();) {
        it->x += kDashSpeed * it->dir;
        ForEachZombie(S.board, [&](uintptr_t z, int r, float cx) {
            if (r == it->row && std::fabs(cx - it->x) < 30 &&
                std::find(it->hit.begin(), it->hit.end(), z) == it->hit.end()) {
                Damage(z, kBirds[it->kind].damage, it->kind); it->hit.push_back(z);
            }
        });
        bool done = it->dir > 0 ? it->x > it->endX : it->x < it->endX;
        if (done) it = S.dashes.erase(it); else ++it;
    }
    for (auto it = S.traps.begin(); it != S.traps.end();) {
        bool alive = !pvz::rd<uint8_t>(it->zombie + pvz::Zombie_Dead) && pvz::rd<int>(it->zombie + pvz::Zombie_BodyHp) > 0;
        if (alive) pvz::wr<float>(it->zombie + pvz::Zombie_PosX, it->x);
        if (!alive || --it->ticks <= 0) it = S.traps.erase(it); else ++it;
    }
    for (auto it = S.fx.begin(); it != S.fx.end();) { if (++it->t > it->life) it = S.fx.erase(it); else ++it; }
}

void ResetForBoard(uintptr_t b) {
    S = State{};
    S.board = b;
    // Expensive birds start part-charged, like Cherry Bomb.
    for (int i = 0; i < kBirdCount; ++i) if (kBirds[i].rechargeTicks > 750) S.recharge[i] = kBirds[i].rechargeTicks / 2;
    if (b) Log("level started, board %08x", (unsigned)b);
}

// Called once per presented frame.
void Update() {
    uintptr_t b = pvz::Board();
    if (b != S.board) ResetForBoard(b);
    if (!b) return;
    SyncChooser();
    int clock = pvz::rd<int>(b + pvz::Board_Clock);
    if (S.lastClock < 0) S.lastClock = clock;
    int ticks = clock - S.lastClock;
    if (ticks < 0 || ticks > 50) ticks = 0;       // level restart / long stall
    S.lastClock = clock;
    if (ticks > 0 && !S.levelChecked) {
        // First tick of play. Early Adventure levels (1-1 to 1-9, not 1-5 bowling) have no
        // seed chooser, so the player gets Red in the slot after their plants.
        S.levelChecked = true;
        int level = pvz::rd<int>(b + pvz::Board_Level);
        uintptr_t bank = Bank();
        Log("level %d playing, chooser used: %s", level, S.chooserSeen ? "yes" : "no");
        if (!S.chooserSeen && bank && level >= 1 && level <= 9 && level != 5) {
            int plants = pvz::rd<int>(bank + pvz::Bank_NumPackets);
            S.order.assign(plants, -1);
            S.order.push_back(Red);
            S.origPackets = plants + 1;
            pvz::wr<int>(bank + pvz::Bank_NumPackets, plants + 1);   // one more slot in the tray, for Red
            S.levelSetup = true;
            Log("early level: Red added after %d plant slot(s)", plants);
        }
    }
    if (ticks > 0) { S.running = true; S.idleFrames = 0; }
    else if (++S.idleFrames > 30) S.running = false;   // paused, menu, or seed picker
    while (ticks-- > 0) Tick();
}

// ---------- launching ----------
void AimTarget(float& tx, float& ty) {
    float dx = kSlingX - S.pullX, dy = kSlingY - S.pullY;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len > kMaxPull) { dx *= kMaxPull / len; dy *= kMaxPull / len; }
    // A full pull straight back reaches the right edge of the lawn.
    float scale = (kLawnRight - 20 - kSlingX) / kMaxPull;
    tx = std::clamp(kSlingX + dx * scale, kLawnLeft + 10, kLawnRight - 10);
    float bottom = kLawnTop + RowCount(S.board) * RowHeight(S.board);
    ty = std::clamp(kSlingY + dy * scale * 0.6f, kLawnTop + 10, bottom - 10);
}

void ClampPull(float x, float y) {
    float dx = x - kSlingX, dy = y - kSlingY, len = std::sqrt(dx * dx + dy * dy);
    if (len > kMaxPull) { dx *= kMaxPull / len; dy *= kMaxPull / len; }
    S.pullX = kSlingX + dx; S.pullY = kSlingY + dy;
}

bool CanAfford(int bird) {
    return S.running && S.recharge[bird] == 0 && pvz::rd<int>(S.board + pvz::Board_Sun) >= kBirds[bird].cost;
}

void RemoveBeltBird(int bird);

void Launch() {
    const BirdDef& d = kBirds[S.loaded];
    bool belt = S.loadedFromBelt;
    if (belt ? !S.running : !CanAfford(S.loaded)) { S.loaded = -1; S.loadedFromBelt = false; return; }
    if (belt) RemoveBeltBird(S.loaded);                        // free, like belt plants
    else pvz::wr<int>(S.board + pvz::Board_Sun, pvz::rd<int>(S.board + pvz::Board_Sun) - d.cost);
    float tx, ty; AimTarget(tx, ty);
    int dur = int(30 + (tx - kSlingX) / 12);       // 0.3–0.9 s of game time
    BirdKind k = BirdKind(S.loaded);
    S.shots.push_back({k, kSlingX, kSlingY - 20, tx, ty, 0, dur, true});
    if (d.effect == Split3) {                      // Blues: three birds, rows above and below
        float rh = RowHeight(S.board), bottom = kLawnTop + RowCount(S.board) * rh;
        if (ty - rh > kLawnTop) S.shots.push_back({k, kSlingX, kSlingY - 20, tx, ty - rh, 0, dur, false});
        if (ty + rh < bottom)   S.shots.push_back({k, kSlingX, kSlingY - 20, tx, ty + rh, 0, dur, false});
    }
    if (!belt) S.recharge[S.loaded] = d.rechargeTicks;
    assets::Snd(d.flySnd).Play();
    Log("launch: %s%s aimed at x %d y %d, sun left %d", d.name, belt ? " (from belt)" : "", int(tx), int(ty),
        pvz::rd<int>(S.board + pvz::Board_Sun));
    S.loaded = -1;
    S.loadedFromBelt = false;
}

// ---------- drawing ----------
void Blit(HDC dc, const Sprite& s, float cx, float cy, float scale = 1.f, BYTE alpha = 255) {
    if (!s.ok()) return;
    int w = int(s.w * scale), h = int(s.h * scale);
    BLENDFUNCTION bf{AC_SRC_OVER, 0, alpha, AC_SRC_ALPHA};
    AlphaBlend(dc, int(cx - s.pivotX * scale), int(cy - s.pivotY * scale), w, h, s.dc, 0, 0, s.w, s.h, bf);
}
// Draws a sprite scaled to fit a box, centred in it.
void BlitFit(HDC dc, const Sprite& s, RECT box, BYTE alpha = 255) {
    if (!s.ok()) return;
    float sc = std::min(float(box.right - box.left) / s.w, float(box.bottom - box.top) / s.h);
    int w = int(s.w * sc), h = int(s.h * sc);
    BLENDFUNCTION bf{AC_SRC_OVER, 0, alpha, AC_SRC_ALPHA};
    AlphaBlend(dc, (box.left + box.right - w) / 2, (box.top + box.bottom - h) / 2, w, h, s.dc, 0, 0, s.w, s.h, bf);
}

void ShadeRect(HDC dc, RECT r, BYTE alpha) {
    static HDC black = [] {
        HDC d = CreateCompatibleDC(nullptr);
        BITMAPINFO bi{}; bi.bmiHeader = {sizeof(BITMAPINFOHEADER), 1, 1, 1, 32, BI_RGB};
        void* bits; HBITMAP bm = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        *static_cast<uint32_t*>(bits) = 0; SelectObject(d, bm); return d;
    }();
    BLENDFUNCTION bf{AC_SRC_OVER, 0, alpha, 0};
    AlphaBlend(dc, r.left, r.top, r.right - r.left, r.bottom - r.top, black, 0, 0, 1, 1, bf);
}

// A bird seed packet drawn the way the game draws plant packets: its own packet
// background (from main.pak), the bird in the art window, the cost along the bottom.
// shade: alpha of a dark overlay (0 = none); cooldown: fraction still recharging.
void DrawBirdPacket(HDC dc, RECT r, int bird, BYTE shade, float cooldown, bool showCost = true) {
    const Sprite& bg = assets::PacketBackground();
    if (bg.ok()) {
        BLENDFUNCTION bf{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        AlphaBlend(dc, r.left, r.top, r.right - r.left, r.bottom - r.top, bg.dc, 0, 0, bg.w, bg.h, bf);
    }
    RECT art{r.left + 6, r.top + 9, r.right - 6, r.top + 50};
    BlitFit(dc, assets::Get(kBirds[bird].idle), art);
    HFONT font = CreateFontA(12, 0, 0, 0, FW_BOLD, 0, 0, 0, ANSI_CHARSET, 0, 0, NONANTIALIASED_QUALITY, 0, "Arial");
    HGDIOBJ of = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(0, 0, 0));
    char cost[8]; wsprintfA(cost, "%d", kBirds[bird].cost);
    RECT tr{r.left + 2, r.bottom - 17, r.left + 33, r.bottom - 4};   // the game prints it left of the sun icon
    if (showCost) DrawTextA(dc, cost, -1, &tr, DT_RIGHT | DT_SINGLELINE);   // belt packets have no cost
    SelectObject(dc, of); DeleteObject(font);
    if (shade) ShadeRect(dc, r, shade);
    if (cooldown > 0) { RECT c = r; c.bottom = r.top + LONG((r.bottom - r.top) * cooldown); ShadeRect(dc, c, 90); }
}

void Line(HDC dc, float x0, float y0, float x1, float y1) { MoveToEx(dc, int(x0), int(y0), nullptr); LineTo(dc, int(x1), int(y1)); }

void DrawSlingshot(HDC dc) {
    // An original, plain wooden Y-fork.
    HPEN wood = CreatePen(PS_SOLID, 7, RGB(120, 72, 30)), band = CreatePen(PS_SOLID, 3, RGB(60, 30, 15));
    HGDIOBJ old = SelectObject(dc, wood);
    float bx = kSlingX, by = kSlingY + 55;
    Line(dc, bx, by, bx, kSlingY + 15);
    Line(dc, bx, kSlingY + 15, bx - 12, kSlingY - 22);
    Line(dc, bx, kSlingY + 15, bx + 12, kSlingY - 22);
    if (S.loaded >= 0) {
        float px = S.dragging ? S.pullX : kSlingX, py = S.dragging ? S.pullY : kSlingY - 20;
        SelectObject(dc, band);
        Line(dc, bx + 12, kSlingY - 20, px, py);
        Blit(dc, assets::Get(kBirds[S.loaded].idle), px, py, 0.8f);
        Line(dc, bx - 12, kSlingY - 20, px, py);
        if (S.dragging) {
            float tx, ty; AimTarget(tx, ty);
            HBRUSH dot = CreateSolidBrush(RGB(255, 255, 255));
            HGDIOBJ ob = SelectObject(dc, dot); SelectObject(dc, GetStockObject(NULL_PEN));
            for (int i = 1; i <= 14; ++i) {
                float t = i / 15.f, x = kSlingX + (tx - kSlingX) * t;
                float y = kSlingY - 20 + (ty - kSlingY + 20) * t - std::sin(t * 3.14159f) * 120;
                Ellipse(dc, int(x - 3), int(y - 3), int(x + 3), int(y + 3));
            }
            HPEN ring = CreatePen(PS_SOLID, 2, RGB(255, 255, 255));
            SelectObject(dc, ring); SelectObject(dc, GetStockObject(NULL_BRUSH));
            Ellipse(dc, int(tx - 18), int(ty - 9), int(tx + 18), int(ty + 9));
            SelectObject(dc, ob); DeleteObject(dot); DeleteObject(ring);
        }
    }
    SelectObject(dc, old); DeleteObject(wood); DeleteObject(band);
}

void Ring(HDC dc, float x, float y, float r, COLORREF c, int width) {
    HPEN pen = CreatePen(PS_SOLID, width, c);
    HGDIOBJ op = SelectObject(dc, pen), ob = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Ellipse(dc, int(x - r), int(y - r * 0.6f), int(x + r), int(y + r * 0.6f));
    SelectObject(dc, op); SelectObject(dc, ob); DeleteObject(pen);
}

void DrawEffects(HDC dc) {
    for (auto& s : S.shots) {
        float t = float(s.t) / s.dur;
        float x = s.x0 + (s.x1 - s.x0) * t, y = s.y0 + (s.y1 - s.y0) * t - std::sin(t * 3.14159f) * 120;
        const Sprite& fly = assets::Get(kBirds[s.kind].flying);
        Blit(dc, fly.ok() ? fly : assets::Get(kBirds[s.kind].idle), x, y);
    }
    for (auto& d : S.dashes) {
        const Sprite& sp = assets::Get(kBirds[d.kind].special);
        Blit(dc, sp.ok() ? sp : assets::Get(kBirds[d.kind].idle), d.x, d.y);
    }
    for (auto& t : S.traps) {
        float y = 0; int row = 0;
        (void)row;
        y = pvz::rd<float>(t.zombie + pvz::Zombie_PosY) + 60;
        Ring(dc, t.x + 60, y, 45, RGB(255, 150, 210), 3);
    }
    HFONT font = CreateFontA(20, 0, 0, 0, FW_HEAVY, 0, 0, 0, ANSI_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, "Arial");
    HGDIOBJ of = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    for (auto& f : S.fx) {
        float p = float(f.t) / f.life;
        switch (f.kind) {
        case Fx::Sprite: {
            const Sprite& sp = assets::Get(kBirds[f.bird].special);
            Blit(dc, sp.ok() ? sp : assets::Get(kBirds[f.bird].idle), f.x, f.y, 1.f, BYTE(255 * (1 - p)));
            break;
        }
        case Fx::Ring:
            Ring(dc, f.x, f.y, f.value * (0.4f + 0.8f * p), RGB(255, 255, 255), std::max(1, int(5 * (1 - p))));
            break;
        case Fx::Blast:
            Ring(dc, f.x, f.y, f.value * (0.3f + p), RGB(255, 200, 60), 8);
            Ring(dc, f.x, f.y, f.value * (0.2f + 0.8f * p), RGB(255, 120, 30), 5);
            break;
        case Fx::Number: {
            char txt[16]; wsprintfA(txt, "-%d", f.value);
            int y = int(f.y - 30 * p);
            SetTextColor(dc, RGB(60, 0, 0)); TextOutA(dc, int(f.x) + 1, y + 1, txt, lstrlenA(txt));
            SetTextColor(dc, RGB(255, 60, 40)); TextOutA(dc, int(f.x), y, txt, lstrlenA(txt));
            break;
        }
        default: break;
        }
    }
    SelectObject(dc, of); DeleteObject(font);
}

// ---------- seed chooser + seed bank ----------
// The seed bank is one list of slots in pick order (S.order): -1 is a plant,
// 0..kBirdCount-1 a bird. The game only knows its plants, which it keeps packed
// by index 0..P-1; the mod moves each plant's packet to the slot of the k-th
// plant entry in S.order, and draws birds in their own slots with the game's
// packet art. While choosing, the bank's slot count is lowered by the number of
// birds so the game stops plant picks when the bank is full.
constexpr int kPanelX = 470, kPanelY = 118, kPanelCols = 2;
constexpr int kPanelW = kPanelCols * 53 + 14, kPanelH = 30 + ((kBirdCount + 1) / 2) * 70 + 8;
constexpr int kChosenSeeds = 49, kChosenStride = 0x3C;
constexpr uint32_t Seed_X = 0x00, Seed_EndX = 0x18, Seed_Type = 0x20, Seed_State = 0x24, Seed_BankIdx = 0x28;
enum { SeedFlyingToBank = 0, SeedInBank = 1, SeedInChooser = 3 };
constexpr uint32_t Chooser_ImitaterButton = 0xB8;   // GameButton*, x/y/w/h at +0x10..+0x1C
constexpr uint32_t Chooser_Seeds = 0xBC;
constexpr uint32_t Packet_Base = 0x28, Packet_Stride = 0x50, Packet_X = 0x08;

uintptr_t Chooser() { uintptr_t a = pvz::App(); return a ? pvz::rd<uintptr_t>(a + pvz::App_SeedChooser) : 0; }
uintptr_t Bank() { return S.board ? pvz::rd<uintptr_t>(S.board + pvz::Board_SeedBank) : 0; }
int& BankSlots(uintptr_t bank) { return *reinterpret_cast<int*>(bank + pvz::Bank_NumPackets); }

int BirdCount() { return int(std::count_if(S.order.begin(), S.order.end(), [](int e) { return e >= 0; })); }
bool IsChosen(int bird) { return std::find(S.order.begin(), S.order.end(), bird) != S.order.end(); }
// Slot holding the k-th plant (by the game's bank index).
int PlantSlot(int k) {
    for (int i = 0, seen = 0; i < int(S.order.size()); ++i)
        if (S.order[i] < 0 && seen++ == k) return i;
    return k + BirdCount();   // not tracked yet: after everything
}
int SlotX(uintptr_t bank, int slot) { return pvz::rd<int>(bank + pvz::Bank_X) + pvz::SlotFirstX + slot * pvz::SlotStepX; }
RECT SlotRect(int slot) {
    uintptr_t bank = Bank();
    int x = SlotX(bank, slot), y = pvz::rd<int>(bank + pvz::Bank_Y) + pvz::SlotY;
    return {x, y, x + pvz::PacketW, y + pvz::PacketH};
}

POINT ChooserOrigin(uintptr_t c) { return {pvz::rd<int>(c + pvz::Chooser_X), pvz::rd<int>(c + pvz::Chooser_Y)}; }
RECT PanelPacketRect(uintptr_t c, int bird) {
    POINT o = ChooserOrigin(c);
    int x = o.x + kPanelX + 7 + (bird % kPanelCols) * 53, y = o.y + kPanelY + 30 + (bird / kPanelCols) * 70;
    return {x, y, x + pvz::PacketW, y + pvz::PacketH};
}
uintptr_t ChosenSeed(uintptr_t c, int i) { return c + Chooser_Seeds + i * kChosenStride; }

// Follows the game's plant picks/removals so S.order stays in step, then lays the bank out.
void TrackChooserPlants(uintptr_t c) {
    std::map<int, int> now;   // seed type -> bank index, for plants in or flying to the bank
    for (int i = 0; i < kChosenSeeds; ++i) {
        uintptr_t s = ChosenSeed(c, i);
        int st = pvz::rd<int>(s + Seed_State);
        if (st == SeedInBank || st == SeedFlyingToBank) now[pvz::rd<int>(s + Seed_Type)] = pvz::rd<int>(s + Seed_BankIdx);
    }
    // Removed plants: drop the plant entry at their old index (highest index first).
    std::vector<int> gone;
    for (auto& [type, idx] : S.plantIdx) if (!now.count(type)) gone.push_back(idx);
    std::sort(gone.rbegin(), gone.rend());
    for (int k : gone) {
        for (int i = 0, seen = 0; i < int(S.order.size()); ++i)
            if (S.order[i] < 0 && seen++ == k) { S.order.erase(S.order.begin() + i); break; }
    }
    // New plants take the next slot, like the game does.
    std::vector<std::pair<int, int>> added;
    for (auto& [type, idx] : now) if (!S.plantIdx.count(type)) added.push_back({idx, type});
    std::sort(added.begin(), added.end());
    for (size_t n = 0; n < added.size(); ++n) S.order.push_back(-1);
    S.plantIdx = now;

    uintptr_t bank = Bank();
    for (int i = 0; i < kChosenSeeds; ++i) {
        uintptr_t s = ChosenSeed(c, i);
        int st = pvz::rd<int>(s + Seed_State);
        if (st != SeedInBank && st != SeedFlyingToBank) continue;
        int x = SlotX(bank, PlantSlot(pvz::rd<int>(s + Seed_BankIdx))) - ChooserOrigin(c).x;
        pvz::wr<int>(s + Seed_EndX, x);
        if (st == SeedInBank) pvz::wr<int>(s + Seed_X, x);
    }
}

// Each frame while the chooser is open: plant cap, Let's Rock, plant positions.
void SyncChooser() {
    uintptr_t c = Chooser(), bank = Bank();
    if (!c || !bank) {
        if (S.chooserSeen && bank && !S.levelSetup) {
            // Chooser closed: the level is starting. The bank kept all its slots throughout;
            // the packets the plants don't use sit under the bird slots (LayoutLevelBank).
            S.levelSetup = true;
            Log("level layout: %d slots, %d bird(s)", BankSlots(bank), BirdCount());
        }
        return;
    }
    if (!S.chooserSeen) { S.chooserSeen = true; S.origPackets = BankSlots(bank); Log("seed chooser open, %d slots", S.origPackets); }
    TrackChooserPlants(c);
    // The bank's slot count is never changed (the game sizes the tray from it); the plant
    // cap with birds in the bank is enforced by blocking grid clicks (HandleChooserMouse).
    int want = S.origPackets - BirdCount();
    if (BirdCount()) {
        uintptr_t btn = pvz::rd<uintptr_t>(c + pvz::Chooser_StartButton);
        int plants = pvz::rd<int>(c + pvz::Chooser_SeedsInBank);
        if (btn) pvz::wr<uint8_t>(btn + pvz::Button_Disabled, plants < want ? 1 : 0);
    }
}

// In the level: put each plant packet in its slot and park the spare packets under the birds.
void LayoutLevelBank() {
    uintptr_t bank = Bank();
    if (!bank || S.order.empty()) return;
    int slots = BankSlots(bank), plants = int(S.order.size()) - BirdCount();
    std::vector<int> birdSlots;
    for (int i = 0; i < int(S.order.size()); ++i) if (S.order[i] >= 0) birdSlots.push_back(i);
    for (int k = 0, spare = 0; k < slots && k < 10; ++k) {
        int slot = k < plants ? PlantSlot(k) : (spare < int(birdSlots.size()) ? birdSlots[spare++] : k);
        pvz::wr<int>(bank + Packet_Base + k * Packet_Stride + Packet_X, SlotX(bank, slot));
    }
}

bool BankFull(uintptr_t c) { return pvz::rd<int>(c + pvz::Chooser_SeedsInBank) + BirdCount() >= S.origPackets; }

void PickBird(int bird) {
    uintptr_t c = Chooser();
    if (IsChosen(bird) || BankFull(c)) return;
    S.order.push_back(bird);
    assets::Snd(kBirds[bird].selectSnd).Play();
    Log("chooser: picked %s into slot %d", kBirds[bird].name, int(S.order.size()) - 1);
    SyncChooser();
}
void UnpickBird(int bird) {
    S.order.erase(std::remove(S.order.begin(), S.order.end(), bird), S.order.end());
    Log("chooser: removed %s", kBirds[bird].name);
    SyncChooser();
    uintptr_t c = Chooser(), btn = c ? pvz::rd<uintptr_t>(c + pvz::Chooser_StartButton) : 0;
    if (btn && pvz::rd<int>(c + pvz::Chooser_SeedsInBank) < S.origPackets - BirdCount()) pvz::wr<uint8_t>(btn + pvz::Button_Disabled, 1);
}

void DrawChooserPanel(HDC dc, uintptr_t c) {
    POINT o = ChooserOrigin(c);
    RECT panel{o.x + kPanelX, o.y + kPanelY, o.x + kPanelX + kPanelW, o.y + kPanelY + kPanelH};
    HBRUSH fill = CreateSolidBrush(RGB(124, 70, 32)), inner = CreateSolidBrush(RGB(70, 40, 20)), edge = CreateSolidBrush(RGB(50, 28, 10));
    FillRect(dc, &panel, fill); FrameRect(dc, &panel, edge);
    RECT well{panel.left + 4, panel.top + 26, panel.right - 4, panel.bottom - 4};
    FillRect(dc, &well, inner);
    HFONT font = CreateFontA(16, 0, 0, 0, FW_HEAVY, 0, 0, 0, ANSI_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, "Arial");
    HGDIOBJ of = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, RGB(240, 200, 80));
    RECT title{panel.left, panel.top + 4, panel.right, panel.top + 24};
    DrawTextA(dc, "CHOOSE BIRDS", -1, &title, DT_CENTER | DT_SINGLELINE);
    SelectObject(dc, of);
    DeleteObject(fill); DeleteObject(inner); DeleteObject(edge); DeleteObject(font);

    bool full = BankFull(c);
    for (int i = 0; i < kBirdCount; ++i) {
        if (IsChosen(i)) ShadeRect(dc, PanelPacketRect(c, i), 120);   // empty place, like a picked plant
        else DrawBirdPacket(dc, PanelPacketRect(c, i), i, full ? 90 : 0, 0);
    }
}

void DrawBankBirds(HDC dc, bool inLevel) {
    for (int slot = 0; slot < int(S.order.size()); ++slot) {
        int bird = S.order[slot];
        if (bird < 0) continue;
        BYTE shade = 0; float cool = 0;
        if (inLevel) {
            cool = S.recharge[bird] > 0 ? float(S.recharge[bird]) / kBirds[bird].rechargeTicks : 0;
            if (cool || S.loaded == bird || pvz::rd<int>(S.board + pvz::Board_Sun) < kBirds[bird].cost) shade = 110;
        }
        DrawBirdPacket(dc, SlotRect(slot), bird, shade, cool);
    }
}

// ---------- conveyor belt ----------
// In conveyor levels (no seed chooser, packets slide in from the right) some new
// arrivals become birds. A bird packet is marked by giving it an upgrade-plant seed
// type (40-47, one per bird), which never comes down an Adventure belt; the game then
// slides and shifts it like any packet, and the mod draws the bird over it and takes
// its clicks. Belt birds are free and have no recharge, like belt plants.
constexpr int kBeltMarker = 40, kBeltSlots = 10, kBeltStep = 50, kBeltChance = 4, kMaxBeltBirds = 2;
constexpr uint32_t Packet_Y = 0x0C, Packet_OffX = 0x30, Packet_Type = 0x34, Packet_Imitater = 0x38;

uintptr_t BeltPacket(uintptr_t bank, int i) { return bank + Packet_Base + i * Packet_Stride; }
int BeltBird(uintptr_t p) {
    int t = pvz::rd<int>(p + Packet_Type);
    return (t >= kBeltMarker && t < kBeltMarker + kBirdCount) ? t - kBeltMarker : -1;
}
RECT BeltRect(uintptr_t p) {
    int x = pvz::rd<int>(p + Packet_X) + pvz::rd<int>(p + Packet_OffX), y = pvz::rd<int>(p + Packet_Y);
    return {x, y, x + pvz::PacketW, y + pvz::PacketH};
}

void UpdateBelt() {
    uintptr_t bank = Bank();
    if (!bank) return;
    if (!S.conveyor) {
        int level = pvz::rd<int>(S.board + pvz::Board_Level);
        // Only belt packets ever have a slide offset, so that alone marks a conveyor level.
        // (Conveyor levels still create a hidden seed chooser, so chooserSeen can't be used.)
        if (level == 5 || !S.order.empty()) return;              // 1-5 is wall-nut bowling
        bool sliding = false;
        for (int i = 0; i < kBeltSlots; ++i) sliding |= pvz::rd<int>(BeltPacket(bank, i) + Packet_OffX) != 0;
        if (!sliding) return;
        S.conveyor = true;
        Log("conveyor level %d: birds will join the belt", level);
    }
    int count = 0, birds = 0;
    while (count < kBeltSlots && pvz::rd<int>(BeltPacket(bank, count) + Packet_Type) >= 0) {
        if (BeltBird(BeltPacket(bank, count)) >= 0) ++birds;
        ++count;
    }
    if (S.beltCount >= 0)
        for (int i = S.beltCount; i < count; ++i) {               // new arrivals at the end
            uintptr_t p = BeltPacket(bank, i);
            if (BeltBird(p) >= 0 || birds >= kMaxBeltBirds || rand() % kBeltChance) continue;
            int bird = rand() % kBirdCount;
            pvz::wr<int>(p + Packet_Type, kBeltMarker + bird);
            pvz::wr<int>(p + Packet_Imitater, -1);
            ++birds;
            Log("belt: %s arrives in slot %d", kBirds[bird].name, i);
        }
    S.beltCount = count;
}

// Takes a bird packet off the belt the way the game removes a planted one: the packets
// after it move down a slot and keep their screen position (offset + one slot), then slide on.
void RemoveBeltBird(int bird) {
    uintptr_t bank = Bank();
    if (!bank) return;
    int i = 0;
    while (i < kBeltSlots && BeltBird(BeltPacket(bank, i)) != bird) ++i;
    if (i == kBeltSlots) return;
    for (; i < kBeltSlots; ++i) {
        uintptr_t p = BeltPacket(bank, i);
        uintptr_t next = i + 1 < kBeltSlots ? BeltPacket(bank, i + 1) : 0;
        if (!next || pvz::rd<int>(next + Packet_Type) < 0) {
            pvz::wr<int>(p + Packet_Type, -1);
            pvz::wr<int>(p + Packet_Imitater, -1);
            pvz::wr<int>(p + Packet_OffX, 0);
            break;
        }
        pvz::wr<int>(p + Packet_Type, pvz::rd<int>(next + Packet_Type));
        pvz::wr<int>(p + Packet_Imitater, pvz::rd<int>(next + Packet_Imitater));
        pvz::wr<int>(p + Packet_OffX, pvz::rd<int>(next + Packet_OffX) + kBeltStep);
    }
    if (S.beltCount > 0) --S.beltCount;
}

void DrawBeltBirds(HDC dc) {
    uintptr_t bank = Bank();
    for (int i = 0; i < kBeltSlots; ++i) {
        uintptr_t p = BeltPacket(bank, i);
        int bird = BeltBird(p);
        if (bird < 0) continue;
        BYTE shade = (S.loaded == bird && S.loadedFromBelt) ? 110 : 0;
        DrawBirdPacket(dc, BeltRect(p), bird, shade, 0, false);
    }
}

void DrawOverlay(HDC dc) {
    if (!S.board) return;
    uintptr_t bank = Bank();
    if (!bank || !pvz::rd<uint8_t>(bank + pvz::Bank_Visible)) return;
    uintptr_t c = Chooser();
    if (c) { DrawChooserPanel(dc, c); DrawBankBirds(dc, false); return; }
    UpdateBelt();
    if (!BirdCount() && !S.conveyor) return;
    LayoutLevelBank();
    DrawBankBirds(dc, true);
    if (S.conveyor) DrawBeltBirds(dc);
    DrawSlingshot(dc);
    DrawEffects(dc);
}

// ---------- bird almanac ----------
// Adds a "View Birds" panel to PvZ's Almanac index page and a Birds page drawn over the
// Almanac. All text here is written for this mod; stats come from kBirds.
struct AlmanacText { const char* tagline; const char* area; const char* blurb; };
const AlmanacText kAlmanac[kBirdCount] = {
    {"Hits hard where he lands.", "One spot in one lane.",
     "Red has exactly one plan, and it's landing on zombies. He'd explain it, but he's already in the air."},
    {"Lands, then races down the lane.", "Everything to his right in that lane.",
     "Chuck has never once waited for the slingshot to finish stretching. Point him at a lane and stand back."},
    {"Explodes on landing.", "3x3 squares.",
     "Bomb is calm and polite right up until he touches the ground. The neighbours have asked him to stop practising in the driveway."},
    {"Splits into three.", "His lane and the lanes above and below.",
     "Three small birds for the price of one. They argue the whole way across the lawn and still land in formation."},
    {"Boomerangs back toward the house.", "His lane, from where he lands back to the house.",
     "Hal flies out, changes his mind, and comes back the other way. Zombies he passes on the return trip rarely see it coming."},
    {"Flattens a whole column.", "One column, every lane.",
     "Terence doesn't fly so much as get thrown. Whatever is under him when he comes down stops being a problem."},
    {"Puffs up and shoves zombies back.", "3x3 squares.",
     "Bubbles is small until he isn't. Zombies nearby get a light bruise and a long walk back to where they were."},
    {"Traps zombies in bubbles.", "3x3 squares, for 5 seconds.",
     "Stella seals zombies in bubbles for a few seconds. They can't take a step until it pops, which buys your plants some quiet."},
};

struct AlmanacState { bool birdsPage = false; int selected = 0; int lastIndex = -1; } A;
const RECT kViewBirdsPanel{250, 432, 550, 556}, kViewBirdsButton{322, 504, 478, 546};
const RECT kBirdsIndexButton{32, 567, 196, 593}, kAlmanacClose{676, 567, 765, 593};

bool Readable(uintptr_t p) {
    MEMORY_BASIC_INFORMATION mbi;
    return p && VirtualQuery(reinterpret_cast<void*>(p), &mbi, sizeof mbi) && mbi.State == MEM_COMMIT &&
           !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD));
}
uintptr_t FindAlmanac() {
    uintptr_t app = pvz::App();
    uintptr_t wm = app ? pvz::rd<uintptr_t>(app + pvz::App_WidgetManager) : 0;
    if (!Readable(wm)) return 0;
    for (uint32_t off : pvz::WidgetMgr_Refs) {
        uintptr_t w = pvz::rd<uintptr_t>(wm + off);
        if (Readable(w) && pvz::rd<uintptr_t>(w) == pvz::kAlmanacVtbl) return w;
    }
    return 0;
}
// The game turns View Plants off on the Plants and Zombies pages.
bool AlmanacOnIndex(uintptr_t alm) {
    uintptr_t b = pvz::rd<uintptr_t>(alm + pvz::Almanac_ViewPlants);
    bool index = Readable(b) && !pvz::rd<uint8_t>(b + pvz::Button_Disabled);
    if (int(index) != A.lastIndex) { A.lastIndex = index; Log("almanac: %s", index ? "index page" : "plants/zombies page"); }
    return index;
}
RECT BirdsPagePacket(int bird) {
    int x = 40 + (bird % 4) * 58, y = 120 + (bird / 4) * 80;
    return {x, y, x + pvz::PacketW, y + pvz::PacketH};
}

void DrawPanelBox(HDC dc, RECT r, COLORREF fill, COLORREF edge) {
    HBRUSH f = CreateSolidBrush(fill), e = CreateSolidBrush(edge);
    FillRect(dc, &r, f);
    for (int i = 0; i < 3; ++i) { RECT b{r.left + i, r.top + i, r.right - i, r.bottom - i}; FrameRect(dc, &b, e); }
    DeleteObject(f); DeleteObject(e);
}
void DrawLabel(HDC dc, RECT r, const char* text, int size, COLORREF color, UINT fmt = DT_CENTER | DT_SINGLELINE | DT_VCENTER) {
    HFONT font = CreateFontA(size, 0, 0, 0, FW_HEAVY, 0, 0, 0, ANSI_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, "Arial");
    HGDIOBJ of = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT); SetTextColor(dc, color);
    DrawTextA(dc, text, -1, &r, fmt);
    SelectObject(dc, of); DeleteObject(font);
}

void DrawViewBirdsPanel(HDC dc) {
    DrawPanelBox(dc, kViewBirdsPanel, RGB(70, 120, 170), RGB(30, 60, 100));
    RECT pic{kViewBirdsPanel.left + 110, kViewBirdsPanel.top + 10, kViewBirdsPanel.right - 110, kViewBirdsPanel.top + 66};
    BlitFit(dc, assets::Get(kBirds[Red].idle), pic);
    DrawPanelBox(dc, kViewBirdsButton, RGB(120, 70, 30), RGB(60, 30, 10));
    DrawLabel(dc, kViewBirdsButton, "View Birds", 18, RGB(240, 200, 80));
}

void DrawBirdsPage(HDC dc) {
    // Parchment over the whole Almanac, leaving the game's Close button showing.
    HBRUSH paper = CreateSolidBrush(RGB(244, 214, 160));
    RECT parts[] = {{0, 0, 800, kAlmanacClose.top}, {0, kAlmanacClose.top, kAlmanacClose.left, 600},
                    {kAlmanacClose.right, kAlmanacClose.top, 800, 600}, {kAlmanacClose.left, kAlmanacClose.bottom, kAlmanacClose.right, 600}};
    for (RECT& r : parts) FillRect(dc, &r, paper);
    DeleteObject(paper);

    RECT title{180, 18, 620, 70};
    DrawPanelBox(dc, title, RGB(140, 150, 160), RGB(70, 75, 85));
    DrawLabel(dc, title, "Bird Almanac", 30, RGB(255, 255, 255));

    for (int i = 0; i < kBirdCount; ++i) {
        RECT r = BirdsPagePacket(i);
        DrawBirdPacket(dc, r, i, 0, 0);
        if (i == A.selected) {
            HBRUSH hl = CreateSolidBrush(RGB(255, 230, 60));
            for (int k = 1; k <= 3; ++k) { RECT b{r.left - k, r.top - k, r.right + k, r.bottom + k}; FrameRect(dc, &b, hl); }
            DeleteObject(hl);
        }
    }

    const BirdDef& d = kBirds[A.selected];
    const AlmanacText& t = kAlmanac[A.selected];
    RECT card{290, 96, 770, 550};
    DrawPanelBox(dc, card, RGB(60, 45, 30), RGB(30, 20, 10));
    RECT pic{310, 116, 470, 276};
    DrawPanelBox(dc, pic, RGB(150, 200, 230), RGB(80, 120, 150));
    RECT picIn{pic.left + 12, pic.top + 12, pic.right - 12, pic.bottom - 12};
    BlitFit(dc, assets::Get(d.idle), picIn);

    RECT name{490, 116, 760, 150};
    DrawLabel(dc, name, d.name, 28, RGB(240, 200, 80), DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    RECT tag{490, 152, 760, 190};
    DrawLabel(dc, tag, t.tagline, 15, RGB(255, 255, 255), DT_LEFT | DT_WORDBREAK);

    char cost[40], recharge[40], damage[48], area[96];
    wsprintfA(cost, "Cost: %d sun", d.cost);
    wsprintfA(recharge, "Recharge: %d.%d seconds", d.rechargeTicks / 100, (d.rechargeTicks / 10) % 10);
    switch (d.effect) {
    case BubbleTrap: lstrcpyA(damage, "Damage: none"); break;
    case PuffPush:   wsprintfA(damage, "Damage: %d, plus a shove", d.damage); break;
    case Split3:     wsprintfA(damage, "Damage: %d per bird", d.damage); break;
    case DashRight: case Boomerang: wsprintfA(damage, "Damage: %d to each zombie", d.damage); break;
    default:         wsprintfA(damage, "Damage: %d", d.damage); break;
    }
    wsprintfA(area, "Area: %s", t.area);
    const char* stats[] = {cost, recharge, damage, area};
    for (int i = 0; i < 4; ++i) {
        RECT r{490, 200 + i * 22, 760, 222 + i * 22};
        DrawLabel(dc, r, stats[i], 14, RGB(230, 220, 190), DT_LEFT | DT_SINGLELINE | DT_VCENTER);
    }
    RECT blurb{310, 300, 750, 530};
    DrawLabel(dc, blurb, t.blurb, 17, RGB(255, 245, 225), DT_LEFT | DT_WORDBREAK);

    DrawPanelBox(dc, kBirdsIndexButton, RGB(120, 70, 30), RGB(60, 30, 10));
    DrawLabel(dc, kBirdsIndexButton, "Almanac Index", 15, RGB(240, 200, 80));
}

void DrawAlmanac(HDC dc, uintptr_t alm) {
    if (A.birdsPage) DrawBirdsPage(dc);
    else if (AlmanacOnIndex(alm)) DrawViewBirdsPanel(dc);
}

bool HandleAlmanacMouse(uintptr_t alm, UINT msg, POINT p) {
    if (!A.birdsPage) {
        if (!AlmanacOnIndex(alm) || !PtInRect(&kViewBirdsPanel, p)) return false;
        if (msg == WM_LBUTTONDOWN && PtInRect(&kViewBirdsButton, p)) {
            A.birdsPage = true;
            Log("almanac: birds page");
        }
        return msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP;
    }
    if (PtInRect(&kAlmanacClose, p)) return false;        // the game's own Close button
    if (msg == WM_LBUTTONDOWN) {
        for (int i = 0; i < kBirdCount; ++i) {
            RECT r = BirdsPagePacket(i);
            if (PtInRect(&r, p)) { A.selected = i; assets::Snd(kBirds[i].selectSnd).Play(); }
        }
        if (PtInRect(&kBirdsIndexButton, p)) A.birdsPage = false;
    }
    return true;   // keep the Almanac underneath from reacting while the Birds page is up
}

// ---------- pop-ups over plant select / the level ----------
// The mod draws after the game's whole frame, so without this the birds would sit on top of
// the pause menu, the Imitater picker and other pop-ups. Each pop-up the widget manager
// reports is left unpainted, so it covers the birds the way it covers the plant cards,
// and bird input is ignored while one is open (the game ignores clicks outside it too).
int OpenPopups(RECT out[2]) {
    uintptr_t app = pvz::App();
    uintptr_t wm = app ? pvz::rd<uintptr_t>(app + pvz::App_WidgetManager) : 0;
    if (!Readable(wm)) return 0;
    int n = 0;
    for (uint32_t off : {pvz::WidgetMgr_BaseModal, pvz::WidgetMgr_Focus}) {
        uintptr_t w = pvz::rd<uintptr_t>(wm + off);
        if (!Readable(w)) continue;
        uintptr_t vt = pvz::rd<uintptr_t>(w);
        if (vt == pvz::kBoardVtbl || vt == pvz::kChooserVtbl || vt == pvz::kAlmanacVtbl) continue;
        if (off == pvz::WidgetMgr_Focus && n && w == pvz::rd<uintptr_t>(wm + pvz::WidgetMgr_BaseModal)) continue;
        int x = pvz::rd<int>(w + pvz::Widget_X), y = pvz::rd<int>(w + pvz::Widget_Y);
        int wd = pvz::rd<int>(w + pvz::Widget_W), ht = pvz::rd<int>(w + pvz::Widget_H);
        if (wd <= 0 || ht <= 0 || wd > 2000 || ht > 2000) continue;
        if (off == pvz::WidgetMgr_Focus && !n) continue;   // a focused widget alone isn't a pop-up
        out[n++] = {x, y, x + wd, y + ht};
    }
    static RECT last{};
    RECT now = n ? out[0] : RECT{};
    if (memcmp(&now, &last, sizeof now)) {
        last = now;
        if (n) Log("pop-up open at (%d,%d) %dx%d: birds drawn around it", now.left, now.top, now.right - now.left, now.bottom - now.top);
        else Log("pop-up closed");
    }
    return n;
}

// ---------- debug log (%TEMP%\birdmod.log) ----------
struct Counters { long blt, bltPrimary, bltFast, flip, renders, getDcFail, noBoard; } C;
void Log(const char* fmt, ...) {
    char path[MAX_PATH]; GetTempPathA(MAX_PATH, path); lstrcatA(path, "birdmod.log");
    HANDLE f = CreateFileA(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    char buf[512]; va_list ap; va_start(ap, fmt); int n = wvsprintfA(buf, fmt, ap); va_end(ap);
    buf[n++] = '\r'; buf[n++] = '\n'; DWORD w; WriteFile(f, buf, n, &w, nullptr); CloseHandle(f);
}
void LogStatsMaybe() {
    static DWORD last; DWORD now = GetTickCount();
    if (now - last < 2000) return; last = now;
    uintptr_t b = pvz::Board();
    Log("blt=%ld bltPrimary=%ld bltFast=%ld flip=%ld renders=%ld getDcFail=%ld board=%08x birds=%d running=%d",
        C.blt, C.bltPrimary, C.bltFast, C.flip, C.renders, C.getDcFail, (unsigned)b, BirdCount(), S.running);
}

// ---------- DirectDraw hook ----------
// IDirectDrawSurface (v1) and IDirectDrawSurface7 share slot numbers for the
// methods used here, so one set of hooks serves both; each vtable keeps its own originals.
enum { kGetAttached = 12, kGetCaps = 14, kGetDC = 17, kReleaseDC = 26, kBlt = 5, kBltFast = 7, kFlip = 11 };
using Fn = void*;
template <class R, class... A> R VCall(void* obj, int slot, A... a) {
    using F = R(WINAPI*)(void*, A...);
    return reinterpret_cast<F>((*reinterpret_cast<void***>(obj))[slot])(obj, a...);
}
struct VHooks { void** vtbl = nullptr; Fn blt = nullptr, bltFast = nullptr, flip = nullptr; } g_v1, g_v7;
VHooks& HooksFor(void* obj) { return *reinterpret_cast<void***>(obj) == g_v1.vtbl ? g_v1 : g_v7; }
bool g_inHook;

bool IsPrimary(void* s) {
    DWORD caps[4] = {};
    return s && SUCCEEDED((VCall<HRESULT, void*>(s, kGetCaps, caps))) && (caps[0] & DDSCAPS_PRIMARYSURFACE);
}

void RenderInto(void* surf) {
    if (g_inHook || !surf) return;
    g_inHook = true;
    Update();
    LogStatsMaybe();
    RehookWindowMaybe();   // full-screen toggles replace the game window
    // While the Almanac is open (main menu or plant select) only Almanac pages are drawn.
    uintptr_t alm = FindAlmanac();
    if (!alm) A.birdsPage = false;
    if (S.board || alm) {
        HDC dc;
        if (SUCCEEDED((VCall<HRESULT, HDC*>(surf, kGetDC, &dc)))) {
            if (alm) DrawAlmanac(dc, alm);
            else {
                RECT popups[2];
                int n = OpenPopups(popups);
                for (int i = 0; i < n; ++i) ExcludeClipRect(dc, popups[i].left, popups[i].top, popups[i].right, popups[i].bottom);
                DrawOverlay(dc);
                SelectClipRgn(dc, nullptr);
            }
            VCall<HRESULT, HDC>(surf, kReleaseDC, dc); ++C.renders;
        } else ++C.getDcFail;
    } else ++C.noBoard;
    g_inHook = false;
}

HRESULT WINAPI HookBlt(void* self, LPRECT dst, void* src, LPRECT srcRect, DWORD flags, LPDDBLTFX fx) {
    ++C.blt;
    if (src && IsPrimary(self)) { ++C.bltPrimary; RenderInto(src); }
    using F = HRESULT(WINAPI*)(void*, LPRECT, void*, LPRECT, DWORD, LPDDBLTFX);
    return reinterpret_cast<F>(HooksFor(self).blt)(self, dst, src, srcRect, flags, fx);
}
HRESULT WINAPI HookBltFast(void* self, DWORD x, DWORD y, void* src, LPRECT srcRect, DWORD flags) {
    ++C.bltFast;
    if (src && IsPrimary(self)) { ++C.bltPrimary; RenderInto(src); }
    using F = HRESULT(WINAPI*)(void*, DWORD, DWORD, void*, LPRECT, DWORD);
    return reinterpret_cast<F>(HooksFor(self).bltFast)(self, x, y, src, srcRect, flags);
}
HRESULT WINAPI HookFlip(void* self, void* target, DWORD flags) {
    ++C.flip;
    DWORD caps[4] = {DDSCAPS_BACKBUFFER};
    void* back = nullptr;
    if (SUCCEEDED((VCall<HRESULT, void*, void**>(self, kGetAttached, caps, &back))) && back) {
        RenderInto(back); VCall<ULONG>(back, 2 /*Release*/);
    }
    using F = HRESULT(WINAPI*)(void*, void*, DWORD);
    return reinterpret_cast<F>(HooksFor(self).flip)(self, target, flags);
}

void Patch(void** slot, void* hook, Fn* orig) {
    DWORD old;
    VirtualProtect(slot, sizeof(void*), PAGE_EXECUTE_READWRITE, &old);
    *orig = *slot; *slot = hook;
    VirtualProtect(slot, sizeof(void*), old, &old);
}
void HookVtable(VHooks& h, void* obj) {
    h.vtbl = *reinterpret_cast<void***>(obj);
    Patch(&h.vtbl[kBlt], (void*)&HookBlt, &h.blt);
    Patch(&h.vtbl[kBltFast], (void*)&HookBltFast, &h.bltFast);
    Patch(&h.vtbl[kFlip], (void*)&HookFlip, &h.flip);
}

bool InstallDDrawHooks() {
    using CreateEx = HRESULT(WINAPI*)(GUID*, LPVOID*, REFIID, IUnknown*);
    HMODULE ddraw = LoadLibraryW(L"ddraw.dll");
    auto create = reinterpret_cast<CreateEx>(GetProcAddress(ddraw, "DirectDrawCreateEx"));
    if (!create) { Log("no DirectDrawCreateEx"); return false; }
    static const GUID IID_DD7 = {0x15e65ec0, 0x3b9c, 0x11d2, {0xb9, 0x2f, 0x00, 0x60, 0x97, 0x97, 0xea, 0x5b}};
    static const GUID IID_DDS1 = {0x6C14DB81, 0xA733, 0x11CE, {0xA5, 0x21, 0x00, 0x20, 0xAF, 0x0B, 0xE5, 0x60}};
    IDirectDraw7* dd = nullptr;
    HRESULT hr = create(nullptr, (LPVOID*)&dd, IID_DD7, nullptr);
    if (FAILED(hr)) { Log("DirectDrawCreateEx failed %08x", (unsigned)hr); return false; }
    dd->SetCooperativeLevel(nullptr, DDSCL_NORMAL);
    DDSURFACEDESC2 sd{}; sd.dwSize = sizeof sd;
    sd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT; sd.dwWidth = sd.dwHeight = 8;
    sd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
    IDirectDrawSurface7* surf = nullptr;
    hr = dd->CreateSurface(&sd, &surf, nullptr);
    if (FAILED(hr)) { Log("CreateSurface failed %08x", (unsigned)hr); dd->Release(); return false; }
    HookVtable(g_v7, surf);
    void* s1 = nullptr;
    if (SUCCEEDED(surf->QueryInterface(IID_DDS1, &s1)) && s1) {
        if (*reinterpret_cast<void***>(s1) != g_v7.vtbl) HookVtable(g_v1, s1);
        VCall<ULONG>(s1, 2);
    }
    Log("hooked ddraw at %08x: v7 vtbl %08x, v1 vtbl %08x", (unsigned)(uintptr_t)ddraw,
        (unsigned)(uintptr_t)g_v7.vtbl, (unsigned)(uintptr_t)g_v1.vtbl);
    surf->Release(); dd->Release();
    return true;
}

// ---------- input ----------
WNDPROC g_origProc;

bool HandleChooserMouse(uintptr_t c, UINT msg, POINT p) {
    POINT o = ChooserOrigin(c);
    RECT panel{o.x + kPanelX, o.y + kPanelY, o.x + kPanelX + kPanelW, o.y + kPanelY + kPanelH};
    if (msg != WM_LBUTTONDOWN) return msg == WM_LBUTTONUP && PtInRect(&panel, p);
    for (int i = 0; i < kBirdCount; ++i) {
        RECT r = PanelPacketRect(c, i);
        if (PtInRect(&r, p)) { if (!IsChosen(i)) PickBird(i); return true; }
    }
    for (int slot = 0; slot < int(S.order.size()); ++slot) {
        RECT r = SlotRect(slot);
        if (S.order[slot] >= 0 && PtInRect(&r, p)) { UnpickBird(S.order[slot]); return true; }
    }
    // Bank full of plants + birds: swallow clicks on plants still in the grid (and the
    // Imitater), since the game itself thinks there are free slots left.
    if (BirdCount() && BankFull(c)) {
        for (int i = 0; i < kChosenSeeds; ++i) {
            uintptr_t s = ChosenSeed(c, i);
            if (pvz::rd<int>(s + Seed_State) != SeedInChooser) continue;
            int x = o.x + pvz::rd<int>(s + Seed_X), y = o.y + pvz::rd<int>(s + 0x04);
            RECT r{x, y, x + pvz::PacketW, y + pvz::PacketH};
            if (PtInRect(&r, p)) return true;
        }
        if (uintptr_t imit = pvz::rd<uintptr_t>(c + Chooser_ImitaterButton)) {
            int x = pvz::rd<int>(imit + 0x10), y = pvz::rd<int>(imit + 0x14);
            RECT r{x, y, x + pvz::rd<int>(imit + 0x18), y + pvz::rd<int>(imit + 0x1C)};
            if (PtInRect(&r, p)) return true;
        }
    }
    return PtInRect(&panel, p) != 0;
}

bool HandleLevelMouse(UINT msg, POINT p) {
    switch (msg) {
    case WM_LBUTTONDOWN:
        if (S.conveyor) {
            uintptr_t bank = Bank();
            for (int i = 0; i < kBeltSlots; ++i) {
                uintptr_t pk = BeltPacket(bank, i);
                int bird = BeltBird(pk);
                RECT r = BeltRect(pk);
                if (bird < 0 || !PtInRect(&r, p)) continue;
                if (S.loaded == bird && S.loadedFromBelt) { S.loaded = -1; S.loadedFromBelt = false; }
                else if (S.running) { S.loaded = bird; S.loadedFromBelt = true; assets::Snd(kBirds[bird].selectSnd).Play(); }
                return true;   // never let the game pick up the marker packet
            }
        }
        for (int slot = 0; slot < int(S.order.size()); ++slot) {
            int bird = S.order[slot];
            RECT r = SlotRect(slot);
            if (bird < 0 || !PtInRect(&r, p)) continue;
            if (S.loaded == bird && !S.loadedFromBelt) S.loaded = -1;
            else if (CanAfford(bird)) { S.loaded = bird; S.loadedFromBelt = false; assets::Snd(kBirds[bird].selectSnd).Play(); }
            return true;
        }
        if (S.loaded >= 0 && S.running && std::hypot(p.x - kSlingX, p.y - (kSlingY - 20)) < 40) {
            S.dragging = true; ClampPull(float(p.x), float(p.y));
            assets::Snd(L"slingshot streched.wav").Play();
            return true;
        }
        return false;
    case WM_MOUSEMOVE:
        if (S.dragging) { ClampPull(float(p.x), float(p.y)); return true; }
        return false;
    case WM_LBUTTONUP:
        if (!S.dragging) return false;
        S.dragging = false;
        if (S.running && std::hypot(S.pullX - kSlingX, S.pullY - kSlingY) > 15) Launch();
        return true;
    case WM_RBUTTONDOWN:
        if (S.loaded >= 0) { S.loaded = -1; S.loadedFromBelt = false; S.dragging = false; return true; }
        return false;
    }
    return false;
}

bool HandleMouse(UINT msg, int x, int y) {
    if (uintptr_t alm = FindAlmanac()) return HandleAlmanacMouse(alm, msg, POINT{x, y});
    if (!S.board) return false;
    { RECT popups[2]; if (OpenPopups(popups)) { S.dragging = false; return false; } }   // a pop-up has the input
    uintptr_t bank = Bank();
    if (!bank || !pvz::rd<uint8_t>(bank + pvz::Bank_Visible)) return false;
    POINT p{x, y};
    if (uintptr_t c = Chooser()) return HandleChooserMouse(c, msg, p);
    if (!BirdCount() && !S.conveyor) return false;
    return HandleLevelMouse(msg, p);
}

LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    if (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) {
        RECT rc; GetClientRect(h, &rc);
        int x = short(LOWORD(l)), y = short(HIWORD(l));
        if (rc.right > 0 && rc.bottom > 0) { x = x * 800 / rc.right; y = y * 600 / rc.bottom; }
        if (HandleMouse(msg, x, y)) {
            if (msg == WM_LBUTTONDOWN && S.dragging) SetCapture(h);
            if (msg == WM_LBUTTONUP) ReleaseCapture();
            return 0;
        }
    }
    // Each hooked window keeps its own original procedure (the game may make a new window).
    WNDPROC orig = reinterpret_cast<WNDPROC>(GetPropW(h, L"birdmod_orig"));
    if (msg == WM_NCDESTROY) RemovePropW(h, L"birdmod_orig");
    return CallWindowProcW(orig ? orig : g_origProc, h, msg, w, l);
}

BOOL CALLBACK FindGameWindow(HWND h, LPARAM out) {
    DWORD pid; GetWindowThreadProcessId(h, &pid);
    if (pid == GetCurrentProcessId() && IsWindowVisible(h) && !GetWindow(h, GW_OWNER)) { *(HWND*)out = h; return FALSE; }
    return TRUE;
}

// Hooks the game window's mouse input. Switching between windowed and full screen makes PvZ
// destroy its window and create a new one, so this runs again every half second (from the
// frame hook, on the game's own thread) and hooks whichever window is current.
void HookGameWindow(HWND wnd) {
    if (!wnd || GetWindowLongPtrW(wnd, GWLP_WNDPROC) == reinterpret_cast<LONG_PTR>(&WndProc)) return;
    WNDPROC orig = reinterpret_cast<WNDPROC>(SetWindowLongPtrW(wnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WndProc)));
    SetPropW(wnd, L"birdmod_orig", reinterpret_cast<HANDLE>(orig));
    if (!g_origProc) g_origProc = orig;
    Log("mouse hooked on window %08x", unsigned(uintptr_t(wnd)));
}
void RehookWindowMaybe() {
    static DWORD last;
    DWORD now = GetTickCount();
    if (now - last < 500) return;
    last = now;
    HWND wnd = nullptr;
    EnumWindows(FindGameWindow, reinterpret_cast<LPARAM>(&wnd));
    HookGameWindow(wnd);
}

void Fail(const std::wstring& msg) { MessageBoxW(nullptr, msg.c_str(), L"PvZ bird mod", MB_ICONWARNING); }

DWORD WINAPI Init(LPVOID) {
    if (!pvz::IsSupportedBuild()) { Fail(L"This Plants vs. Zombies build is not supported (needs Steam GOTY 1.2.0.1096). The bird mod is off."); return 0; }
    // One copy per game, even if the DLL is loaded again under another file name.
    wchar_t mtx[64]; wsprintfW(mtx, L"Local\\pvz_birdmod_%lu", GetCurrentProcessId());
    CreateMutexW(nullptr, FALSE, mtx);
    if (GetLastError() == ERROR_ALREADY_EXISTS) { Log("another copy is already running; this one stays off"); return 0; }
    Log("---- birdmod init, build ok");
    srand(GetTickCount());   // which belt arrivals become birds
    std::wstring err;
    if (!assets::LoadPacketArt(err)) Log("packet art: %ls (falling back to plain cards)", err.c_str());
    err.clear();
    if (!assets::Load(assets::FindAngryBirdsData(), err)) { Fail(L"Angry Birds files not found, so the bird mod is off.\n\n" + err + L"\n\nSet AngryBirdsFolder in birdmod.ini next to birdmod.dll."); return 0; }
    HWND wnd = nullptr;
    for (int i = 0; i < 600 && !wnd; ++i) { EnumWindows(FindGameWindow, (LPARAM)&wnd); if (!wnd) Sleep(100); }
    if (!wnd) { Fail(L"Could not find the game window."); return 0; }
    { char cls[64], title[64]; GetClassNameA(wnd, cls, 64); GetWindowTextA(wnd, title, 64); Log("window %08x class=%s title=%s", (unsigned)(uintptr_t)wnd, cls, title); }
    if (!InstallDDrawHooks()) { Fail(L"Could not hook DirectDraw."); return 0; }
    HookGameWindow(wnd);
    return 0;
}

} // namespace

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        CloseHandle(CreateThread(nullptr, 0, Init, nullptr, 0, nullptr));
    }
    return TRUE;
}
