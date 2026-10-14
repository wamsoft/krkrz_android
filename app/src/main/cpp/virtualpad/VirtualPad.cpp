// VirtualPad プラグイン本体 - 状態 / イベント / 2 つの描画パス。
//
//   - SDL_Renderer 経路 (RenderSDL): SDLDrawDevice 用 (SDL_RenderGeometry で三角形リスト)
//   - OpenGL ES 直接経路 (RenderGL): SDLOGLDrawDevice 用 (glDrawArrays GL_TRIANGLES)
//
// 両パスとも同じレイアウト + 同じ三角形セットを生成し、座標 / 色 を別フォーマット
// (SDL_Vertex の float vs GL の uint8 normalized) に変換するだけ。
//
// ボタンは face (A/B/X/Y) / shoulder (L/R) / D-pad の 3 種 10 個。
// 形状: face = 円、shoulder = 角丸矩形、D-pad = 方向三角形。
// レイアウト座標は「画面短辺 (= min(w, h)) を 1 unit」として持つので、解像度や
// アスペクト比が変わっても円が円のままになる。

#include "VirtualPad.h"

#include <SDL3/SDL.h>
#include <glad/gles2.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// KrkrZ エンジン (sdl3/visual/PostRenderCallback.h) からの forward 宣言
struct SDL_Renderer;
typedef void (*tTVPPostRenderCallback)(SDL_Renderer *renderer, void *userdata);
void TVPRegisterPostRenderCallback(tTVPPostRenderCallback cb, void *userdata);
void TVPUnregisterPostRenderCallback(tTVPPostRenderCallback cb, void *userdata);
typedef void (*tTVPPostRenderCallbackGL)(void *userdata);
void TVPRegisterPostRenderCallbackGL(tTVPPostRenderCallbackGL cb, void *userdata);
void TVPUnregisterPostRenderCallbackGL(tTVPPostRenderCallbackGL cb, void *userdata);

#define VPAD_LOG(...) SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "[vpad] " __VA_ARGS__)

namespace virtualpad {

namespace {

// ============================================================================
// SDL3 仮想ジョイスティック button index
// ============================================================================
// SDL_AttachVirtualJoystick で button_mask に set した bit が SDL_GamepadButton
// enum 順に詰めて joystick button index 0..N-1 に対応する (SDL3 仕様)。
// 下記の順番は SDL_GAMEPAD_BUTTON_SOUTH..DPAD_RIGHT の enum 値の昇順。

constexpr int kBtnA          = 0;   // SOUTH
constexpr int kBtnB          = 1;   // EAST
constexpr int kBtnX          = 2;   // WEST
constexpr int kBtnY          = 3;   // NORTH
constexpr int kBtnL          = 4;   // LEFT_SHOULDER
constexpr int kBtnR          = 5;   // RIGHT_SHOULDER
constexpr int kBtnDpadUp     = 6;
constexpr int kBtnDpadDown   = 7;
constexpr int kBtnDpadLeft   = 8;
constexpr int kBtnDpadRight  = 9;
constexpr int kVirtualButtonCount = 10;

// ============================================================================
// レイアウト定義
// ============================================================================

enum BtnShape : uint8_t {
    SHAPE_FACE     = 0,  // 円 (face button)
    SHAPE_DPAD     = 1,  // 方向三角形
    SHAPE_SHOULDER = 2,  // 角丸矩形 (L/R)
};

enum AnchorH : uint8_t { ANCH_L, ANCH_R };
enum AnchorV : uint8_t { ANCH_T, ANCH_B };

struct ColorRGBA { uint8_t r, g, b, a; };

// face = Xbox 配色、L/R/D-pad = グレー、暗縁 = 黒に近いグレー
constexpr ColorRGBA kColA       = {  78, 196, 110, 255 };  // 緑
constexpr ColorRGBA kColB       = { 224,  82,  82, 255 };  // 赤
constexpr ColorRGBA kColX       = {  82, 138, 224, 255 };  // 青
constexpr ColorRGBA kColY       = { 224, 196,  70, 255 };  // 黄
constexpr ColorRGBA kColGray    = {  92,  98, 112, 255 };
constexpr ColorRGBA kColOutline = {  20,  22,  28, 255 };

struct ButtonDef {
    int      button_index;
    BtnShape shape;
    AnchorH  ah;
    AnchorV  av;
    float    ox, oy;     // 中心位置 (アンカーからのオフセット、unit = min(w,h))
    float    sx, sy;     // 半幅 / 半高 (unit = min(w,h)、SHAPE_FACE は sx=半径=sy)
    uint8_t  dpad_dir;   // 0=up, 1=right, 2=down, 3=left (SHAPE_DPAD のみ意味あり)
    ColorRGBA color;
};

// 横画面 (landscape) を前提のレイアウト。
constexpr ButtonDef kLayout[] = {
    // ---- 上端: L / R ショルダー ----
    { kBtnL, SHAPE_SHOULDER, ANCH_L, ANCH_T, 0.13f, 0.06f, 0.10f, 0.025f, 0, kColGray },
    { kBtnR, SHAPE_SHOULDER, ANCH_R, ANCH_T, 0.13f, 0.06f, 0.10f, 0.025f, 0, kColGray },

    // ---- 右下: face buttons (A/B/X/Y を +字配置) ----
    // 配置中心: (right - 0.20u, bottom - 0.20u)
    { kBtnA, SHAPE_FACE, ANCH_R, ANCH_B, 0.20f, 0.10f, 0.055f, 0.055f, 0, kColA }, // 下
    { kBtnB, SHAPE_FACE, ANCH_R, ANCH_B, 0.10f, 0.20f, 0.055f, 0.055f, 0, kColB }, // 右
    { kBtnX, SHAPE_FACE, ANCH_R, ANCH_B, 0.30f, 0.20f, 0.055f, 0.055f, 0, kColX }, // 左
    { kBtnY, SHAPE_FACE, ANCH_R, ANCH_B, 0.20f, 0.30f, 0.055f, 0.055f, 0, kColY }, // 上

    // ---- 左下: D-pad 4 方向 ----
    { kBtnDpadUp,    SHAPE_DPAD, ANCH_L, ANCH_B, 0.20f, 0.30f, 0.045f, 0.045f, 0, kColGray },
    { kBtnDpadDown,  SHAPE_DPAD, ANCH_L, ANCH_B, 0.20f, 0.10f, 0.045f, 0.045f, 2, kColGray },
    { kBtnDpadLeft,  SHAPE_DPAD, ANCH_L, ANCH_B, 0.10f, 0.20f, 0.045f, 0.045f, 3, kColGray },
    { kBtnDpadRight, SHAPE_DPAD, ANCH_L, ANCH_B, 0.30f, 0.20f, 0.045f, 0.045f, 1, kColGray },
};
constexpr int kLayoutCount = sizeof(kLayout) / sizeof(kLayout[0]);

// ============================================================================
// 状態
// ============================================================================

std::atomic<bool>  g_enabled{false};
std::atomic<float> g_opacity{0.55f};
std::atomic<int>   g_preset{0};
// HitTest で normalize → pixel 変換するために最新の描画サイズをキャッシュ。
std::atomic<int>   g_window_w{0};
std::atomic<int>   g_window_h{0};
// Safe Area (ノッチ / パンチホール / ホームインジケータを避ける余白) を描画ピクセル
// 単位で持つ。描画時に更新し、HitTest も同じ値で行う。
std::atomic<float> g_inset_l{0}, g_inset_t{0}, g_inset_r{0}, g_inset_b{0};

struct State {
    SDL_Joystick    *joystick = nullptr;
    SDL_JoystickID   joystick_id = 0;

    std::unordered_map<SDL_FingerID, int> finger_button;  // finger -> layout index
    int button_press_count[kVirtualButtonCount] = {0};
    // touch から合成された mouse (SDL_TOUCH_MOUSEID) の現在ストリームがボタン上で
    // 始まったか。DOWN で確定し UP まで保持することで、途中でボタン領域を跨いでも
    // 「掴んだ側」を一貫させ、pointer の掴みっぱなし (UP を落として離せない) を防ぐ。
    bool mouse_consumed = false;
};

State *g_state = nullptr;

// ============================================================================
// ジオメトリ計算
// ============================================================================

struct PixelRect {
    float cx, cy;     // 中心
    float hw, hh;     // 半幅 / 半高
};

PixelRect ComputeRect(const ButtonDef &b, int w, int h) {
    float u = (float)(w < h ? w : h);
    PixelRect r;
    r.hw = b.sx * u;
    r.hh = b.sy * u;
    r.cx = (b.ah == ANCH_L) ? g_inset_l.load() + b.ox * u : (float)w - g_inset_r.load() - b.ox * u;
    r.cy = (b.av == ANCH_T) ? g_inset_t.load() + b.oy * u : (float)h - g_inset_b.load() - b.oy * u;
    return r;
}

// ウィンドウの Safe Area を描画ピクセル (w x h) 単位の上下左右余白に換算して保持する。
// Safe Area はウィンドウ座標なので、描画サイズとの比で換算する (high-DPI 対応)。
void UpdateSafeArea(SDL_Window *win, int w, int h) {
    float l = 0, t = 0, r = 0, b = 0;
    int ww = 0, wh = 0;
    SDL_Rect safe;
    if (win && SDL_GetWindowSize(win, &ww, &wh) && ww > 0 && wh > 0 &&
        SDL_GetWindowSafeArea(win, &safe)) {
        float sx = (float)w / (float)ww;
        float sy = (float)h / (float)wh;
        l = (float)safe.x * sx;
        t = (float)safe.y * sy;
        r = (float)(ww - safe.x - safe.w) * sx;
        b = (float)(wh - safe.y - safe.h) * sy;
    }
    g_inset_l.store(l > 0 ? l : 0);
    g_inset_t.store(t > 0 ? t : 0);
    g_inset_r.store(r > 0 ? r : 0);
    g_inset_b.store(b > 0 ? b : 0);
}

bool PointInRect(const PixelRect &r, float px, float py) {
    return (px >= r.cx - r.hw && px <= r.cx + r.hw &&
            py >= r.cy - r.hh && py <= r.cy + r.hh);
}

int HitTest(float nx, float ny) {
    int W = g_window_w.load();
    int H = g_window_h.load();
    if (W <= 0 || H <= 0) return -1;
    float px = nx * (float)W;
    float py = ny * (float)H;
    for (int i = 0; i < kLayoutCount; ++i) {
        PixelRect r = ComputeRect(kLayout[i], W, H);
        if (PointInRect(r, px, py)) return i;
    }
    return -1;
}

// mouse イベント (touch から合成) 用。mouse 座標はウィンドウ論理座標なので、
// ウィンドウサイズで正規化してから finger と同じ HitTest に載せる (レンダ出力
// ピクセルと論理サイズが違う high-DPI でも整合する)。ウィンドウが取れなければ
// レンダ出力ピクセルにフォールバック。
int HitTestMouse(SDL_WindowID win_id, float mx, float my) {
    int ww = 0, wh = 0;
    if (SDL_Window *win = SDL_GetWindowFromID(win_id)) {
        SDL_GetWindowSize(win, &ww, &wh);
    }
    if (ww > 0 && wh > 0) return HitTest(mx / (float)ww, my / (float)wh);
    int W = g_window_w.load(), H = g_window_h.load();
    if (W > 0 && H > 0) return HitTest(mx / (float)W, my / (float)H);
    return -1;
}

// ============================================================================
// ボタン入力管理
// ============================================================================

void PressButton(int btn_index) {
    if (!g_state->joystick) return;
    if (btn_index < 0 || btn_index >= kVirtualButtonCount) return;
    g_state->button_press_count[btn_index]++;
    if (g_state->button_press_count[btn_index] == 1) {
        SDL_SetJoystickVirtualButton(g_state->joystick, btn_index, true);
    }
}

void ReleaseButton(int btn_index) {
    if (!g_state->joystick) return;
    if (btn_index < 0 || btn_index >= kVirtualButtonCount) return;
    if (g_state->button_press_count[btn_index] > 0) {
        g_state->button_press_count[btn_index]--;
        if (g_state->button_press_count[btn_index] == 0) {
            SDL_SetJoystickVirtualButton(g_state->joystick, btn_index, false);
        }
    }
}

// ============================================================================
// イベントフィルタ
// ============================================================================
// 観測だけでは不十分 (engine が同じ touch を Layer.onMouseDown 等に届けてしまう)
// ため、SDL_SetEventFilter で「捨てる」ことができる側に乗せる。enabled 時は
// 「ボタンの上に当たった touch だけ」を drop して仮想ジョイスティックに反映し、
// ボタン外に当たった touch は drop せずそのまま engine (下のレイヤ) に流す。
// SDL は touch を mouse としても合成発火するので (SDL_HINT_TOUCH_MOUSE_EVENTS=1)、
// SDL_TOUCH_MOUSEID の mouse も同じ判定で drop / passthrough する。
// finger は fingerID で個別追跡できるが、合成 mouse は fingerID を持たない。SDL の
// touch→mouse 合成は同時に 1 ポインタしか扱わないので、mouse は 1 本の
// mouse_consumed フラグ (DOWN で確定・UP まで保持) で掴み側を一貫させる。

SDL_EventFilter g_prev_filter = nullptr;
void           *g_prev_filter_ud = nullptr;

bool SDLCALL EventFilter(void *ud, SDL_Event *event) {
    if (g_enabled.load() && g_state && g_state->joystick) {
        switch (event->type) {
            case SDL_EVENT_FINGER_DOWN: {
                int layout_idx = HitTest(event->tfinger.x, event->tfinger.y);
                if (layout_idx >= 0) {
                    // ボタン上: 掴んで drop (engine には渡さない)
                    g_state->finger_button[event->tfinger.fingerID] = layout_idx;
                    PressButton(kLayout[layout_idx].button_index);
                    return false;
                }
                // ボタン外: 下のレイヤへ流す
                break;
            }
            case SDL_EVENT_FINGER_MOTION: {
                // 掴んだ finger のみ drop。ボタン外の finger はドラッグごと流す。
                if (g_state->finger_button.count(event->tfinger.fingerID)) return false;
                break;
            }
            case SDL_EVENT_FINGER_UP:
            case SDL_EVENT_FINGER_CANCELED: {
                auto it = g_state->finger_button.find(event->tfinger.fingerID);
                if (it != g_state->finger_button.end()) {
                    ReleaseButton(kLayout[it->second].button_index);
                    g_state->finger_button.erase(it);
                    return false;
                }
                break;
            }
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                if (event->button.which == SDL_TOUCH_MOUSEID) {
                    int idx = HitTestMouse(event->button.windowID,
                                           event->button.x, event->button.y);
                    g_state->mouse_consumed = (idx >= 0);
                    if (g_state->mouse_consumed) return false;
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if (event->button.which == SDL_TOUCH_MOUSEID) {
                    bool consumed = g_state->mouse_consumed;
                    g_state->mouse_consumed = false;
                    if (consumed) return false;
                }
                break;
            case SDL_EVENT_MOUSE_MOTION:
                if (event->motion.which == SDL_TOUCH_MOUSEID && g_state->mouse_consumed) {
                    return false;
                }
                break;
            default:
                break;
        }
    }
    // 自分が捨てない場合は元のフィルタにチェイン (engine 等が後で
    // SDL_SetEventFilter する可能性は今は無いが、保険として保持する)。
    if (g_prev_filter) {
        return g_prev_filter(g_prev_filter_ud, event);
    }
    return true;
}

void InstallEventFilter() {
    g_prev_filter = nullptr;
    g_prev_filter_ud = nullptr;
    SDL_GetEventFilter(&g_prev_filter, &g_prev_filter_ud);
    SDL_SetEventFilter(EventFilter, nullptr);
}

void UninstallEventFilter() {
    // SDL3 には「filter を外す」専用 API がないので、起動時に控えた前フィルタ
    // (nullptr なら何も無し) に書き戻す。
    SDL_SetEventFilter(g_prev_filter, g_prev_filter_ud);
    g_prev_filter = nullptr;
    g_prev_filter_ud = nullptr;
}

// ============================================================================
// 共通三角形バッファ
// ============================================================================
// 内部の vertex フォーマット (xy + uint8 RGBA)。SDL は float color、GL は
// uint8 normalize なので、出力時に変換する。

struct Vert {
    float x, y;
    uint8_t r, g, b, a;
};

ColorRGBA ApplyAlpha(ColorRGBA c, uint8_t alpha_scale) {
    int a = (int)c.a * (int)alpha_scale / 255;
    return { c.r, c.g, c.b, (uint8_t)a };
}

ColorRGBA Lighten(ColorRGBA c) {
    // 押下時用に白へ 50% 寄せる。色ごとに「明るくなった感」が出る。
    return {
        (uint8_t)((c.r + 255) / 2),
        (uint8_t)((c.g + 255) / 2),
        (uint8_t)((c.b + 255) / 2),
        c.a,
    };
}

inline void PushTri(std::vector<Vert> &out,
                    float x0, float y0, float x1, float y1, float x2, float y2,
                    ColorRGBA c) {
    out.push_back({ x0, y0, c.r, c.g, c.b, c.a });
    out.push_back({ x1, y1, c.r, c.g, c.b, c.a });
    out.push_back({ x2, y2, c.r, c.g, c.b, c.a });
}

// 円 (triangle fan を三角形リスト化)
void AppendCircle(std::vector<Vert> &out,
                  float cx, float cy, float radius,
                  ColorRGBA color, int segments) {
    float prev_x = cx + radius;
    float prev_y = cy;
    for (int i = 1; i <= segments; ++i) {
        float angle = (float)i / (float)segments * 2.0f * (float)M_PI;
        float ex = cx + radius * cosf(angle);
        float ey = cy + radius * sinf(angle);
        PushTri(out, cx, cy, prev_x, prev_y, ex, ey, color);
        prev_x = ex; prev_y = ey;
    }
}

// 方向三角形 (D-pad 矢印)
void AppendArrow(std::vector<Vert> &out,
                 float cx, float cy, float size,
                 ColorRGBA color, int dir) {
    // tip = 先端、l/r = 底辺の左右。dir = 0:up / 1:right / 2:down / 3:left
    float forward  = size * 0.85f;
    float backward = size * 0.50f;
    float halfBase = size * 0.85f;
    float tip_x, tip_y, lx, ly, rx, ry;
    switch (dir) {
        case 0:  // up
            tip_x = cx;            tip_y = cy - forward;
            lx    = cx - halfBase; ly    = cy + backward;
            rx    = cx + halfBase; ry    = cy + backward;
            break;
        case 1:  // right
            tip_x = cx + forward;  tip_y = cy;
            lx    = cx - backward; ly    = cy - halfBase;
            rx    = cx - backward; ry    = cy + halfBase;
            break;
        case 2:  // down
            tip_x = cx;            tip_y = cy + forward;
            lx    = cx + halfBase; ly    = cy - backward;
            rx    = cx - halfBase; ry    = cy - backward;
            break;
        case 3:  // left
            tip_x = cx - forward;  tip_y = cy;
            lx    = cx + backward; ly    = cy + halfBase;
            rx    = cx + backward; ry    = cy - halfBase;
            break;
        default:
            return;
    }
    PushTri(out, tip_x, tip_y, lx, ly, rx, ry, color);
}

// 角丸矩形 (中心 (cx,cy)、半幅 hw、半高 hh、角丸半径 r)。
// 9 領域分解: 中央矩形 + 上下左右の帯 + 4 隅の扇形。
void AppendRoundedRect(std::vector<Vert> &out,
                       float cx, float cy, float hw, float hh, float r,
                       ColorRGBA color, int corner_segs) {
    if (r > hw) r = hw;
    if (r > hh) r = hh;

    float L  = cx - hw, T  = cy - hh, R  = cx + hw, B  = cy + hh;
    float iL = L + r,   iT = T + r,   iR = R - r,   iB = B - r;

    // 中央矩形
    PushTri(out, iL, iT, iR, iT, iR, iB, color);
    PushTri(out, iL, iT, iR, iB, iL, iB, color);
    // 上下左右の帯
    PushTri(out, iL, T,  iR, T,  iR, iT, color);
    PushTri(out, iL, T,  iR, iT, iL, iT, color);
    PushTri(out, iL, iB, iR, iB, iR, B,  color);
    PushTri(out, iL, iB, iR, B,  iL, B,  color);
    PushTri(out, L,  iT, iL, iT, iL, iB, color);
    PushTri(out, L,  iT, iL, iB, L,  iB, color);
    PushTri(out, iR, iT, R,  iT, R,  iB, color);
    PushTri(out, iR, iT, R,  iB, iR, iB, color);

    // 4 隅の扇形 (90 deg fan ずつ)
    struct CornerInfo { float ccx, ccy; float a0; };
    CornerInfo corners[4] = {
        { iL, iT, (float)M_PI },          // 左上
        { iR, iT, -(float)M_PI / 2.0f },  // 右上
        { iR, iB, 0.0f },                 // 右下
        { iL, iB, (float)M_PI / 2.0f },   // 左下
    };
    for (auto &cc : corners) {
        float prev_x = cc.ccx + r * cosf(cc.a0);
        float prev_y = cc.ccy + r * sinf(cc.a0);
        for (int i = 1; i <= corner_segs; ++i) {
            float angle = cc.a0 + (float)i / (float)corner_segs * (float)M_PI / 2.0f;
            float ex = cc.ccx + r * cosf(angle);
            float ey = cc.ccy + r * sinf(angle);
            PushTri(out, cc.ccx, cc.ccy, prev_x, prev_y, ex, ey, color);
            prev_x = ex; prev_y = ey;
        }
    }
}

// レイアウト全体を三角形バッファに展開する。
//   - face button: 暗縁 + 主色の二重円 (ボタン感のためのフチ)
//   - shoulder:    暗縁 + 主色の二重角丸
//   - dpad:        単色三角形 (フチなし、形で方向を示す)
void BuildVertices(std::vector<Vert> &out, int w, int h, uint8_t alpha_scale) {
    out.clear();

    for (int i = 0; i < kLayoutCount; ++i) {
        const ButtonDef &b = kLayout[i];
        PixelRect rect = ComputeRect(b, w, h);
        bool pressed = g_state->button_press_count[b.button_index] > 0;
        ColorRGBA base    = ApplyAlpha(pressed ? Lighten(b.color) : b.color, alpha_scale);
        ColorRGBA outline = ApplyAlpha(kColOutline, alpha_scale);

        switch (b.shape) {
            case SHAPE_FACE: {
                float radius = rect.hw;
                AppendCircle(out, rect.cx, rect.cy, radius,            outline, 36);
                AppendCircle(out, rect.cx, rect.cy, radius * 0.86f,    base,    32);
                break;
            }
            case SHAPE_SHOULDER: {
                float corner = (rect.hh < rect.hw ? rect.hh : rect.hw) * 0.85f;
                AppendRoundedRect(out, rect.cx, rect.cy,
                                  rect.hw, rect.hh, corner,
                                  outline, 8);
                float inset = rect.hh * 0.20f;
                AppendRoundedRect(out, rect.cx, rect.cy,
                                  rect.hw - inset, rect.hh - inset,
                                  corner * 0.85f,
                                  base, 8);
                break;
            }
            case SHAPE_DPAD: {
                AppendArrow(out, rect.cx, rect.cy, rect.hw, base, b.dpad_dir);
                break;
            }
        }
    }
}

// ============================================================================
// SDL_Renderer 経路
// ============================================================================

void RenderSDL(SDL_Renderer *renderer, void *ud) {
    if (!g_enabled.load()) return;
    if (!g_state) return;
    if (!renderer) return;

    int w = 0, h = 0;
    SDL_GetCurrentRenderOutputSize(renderer, &w, &h);
    if (w <= 0 || h <= 0) return;
    g_window_w.store(w);
    g_window_h.store(h);
    UpdateSafeArea(SDL_GetRenderWindow(renderer), w, h);

    uint8_t alpha_scale = (uint8_t)(g_opacity.load() * 255.0f);

    thread_local std::vector<Vert> verts;
    BuildVertices(verts, w, h, alpha_scale);
    if (verts.empty()) return;

    thread_local std::vector<SDL_Vertex> sdl_verts;
    sdl_verts.resize(verts.size());
    for (size_t i = 0; i < verts.size(); ++i) {
        const Vert &v = verts[i];
        SDL_Vertex &sv = sdl_verts[i];
        sv.position.x = v.x;
        sv.position.y = v.y;
        sv.color.r = (float)v.r / 255.0f;
        sv.color.g = (float)v.g / 255.0f;
        sv.color.b = (float)v.b / 255.0f;
        sv.color.a = (float)v.a / 255.0f;
        sv.tex_coord.x = 0.0f;
        sv.tex_coord.y = 0.0f;
    }

    SDL_BlendMode old_blend = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(renderer, &old_blend);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_RenderGeometry(renderer, nullptr,
                       sdl_verts.data(), (int)sdl_verts.size(),
                       nullptr, 0);

    SDL_SetRenderDrawBlendMode(renderer, old_blend);
}

// ============================================================================
// OpenGL ES 経路
// ============================================================================

struct GLState {
    bool   inited = false;
    GLuint program = 0;
    GLuint vbo = 0;
    GLint  loc_screenSize = -1;
    GLint  attr_pos = -1;
    GLint  attr_color = -1;
} g_gl;

const char *kVS_GL = R"(#version 100
attribute vec2 a_pos;
attribute vec4 a_color;
uniform vec2 u_screenSize;
varying vec4 v_color;
void main() {
    vec2 p = a_pos / u_screenSize;
    p.y = 1.0 - p.y;
    p = p * 2.0 - 1.0;
    gl_Position = vec4(p, 0.0, 1.0);
    v_color = a_color;
}
)";

const char *kFS_GL = R"(#version 100
precision mediump float;
varying vec4 v_color;
void main() {
    gl_FragColor = v_color;
}
)";

GLuint CompileShader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        VPAD_LOG("shader compile failed: %s", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

bool InitGL() {
    if (g_gl.inited) return true;

    GLuint vs = CompileShader(GL_VERTEX_SHADER, kVS_GL);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, kFS_GL);
    if (!vs || !fs) return false;
    g_gl.program = glCreateProgram();
    glAttachShader(g_gl.program, vs);
    glAttachShader(g_gl.program, fs);
    glLinkProgram(g_gl.program);
    GLint ok = 0;
    glGetProgramiv(g_gl.program, GL_LINK_STATUS, &ok);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!ok) {
        char log[512];
        glGetProgramInfoLog(g_gl.program, sizeof(log), nullptr, log);
        VPAD_LOG("program link failed: %s", log);
        glDeleteProgram(g_gl.program);
        g_gl.program = 0;
        return false;
    }

    g_gl.loc_screenSize = glGetUniformLocation(g_gl.program, "u_screenSize");
    g_gl.attr_pos       = glGetAttribLocation(g_gl.program, "a_pos");
    g_gl.attr_color     = glGetAttribLocation(g_gl.program, "a_color");

    glGenBuffers(1, &g_gl.vbo);

    g_gl.inited = true;
    return true;
}

void ShutdownGL() {
    if (g_gl.program) glDeleteProgram(g_gl.program);
    if (g_gl.vbo) glDeleteBuffers(1, &g_gl.vbo);
    g_gl = GLState{};
}

// VBO に流す pack。Vert と同じだが「ここは 12 バイト固定」と明示するために別 typedef。
struct VertexGL {
    float    x, y;
    uint8_t  r, g, b, a;
};
static_assert(sizeof(VertexGL) == 12, "VertexGL packing");
static_assert(sizeof(Vert) == sizeof(VertexGL), "Vert and VertexGL must be ABI-compatible");

void RenderGL(void *ud) {
    if (!g_enabled.load()) return;
    if (!g_state) return;
    if (!InitGL()) return;

    GLint vp[4] = {0};
    glGetIntegerv(GL_VIEWPORT, vp);
    int w = vp[2], h = vp[3];
    if (w <= 0 || h <= 0) return;
    g_window_w.store(w);
    g_window_h.store(h);
    UpdateSafeArea(SDL_GL_GetCurrentWindow(), w, h);

    uint8_t alpha_scale = (uint8_t)(g_opacity.load() * 255.0f);

    thread_local std::vector<Vert> verts;
    BuildVertices(verts, w, h, alpha_scale);
    if (verts.empty()) return;

    // 既存 GL state を退避
    GLint prev_program = 0;       glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);
    GLint prev_vbo     = 0;       glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prev_vbo);
    GLboolean prev_blend = glIsEnabled(GL_BLEND);
    GLboolean prev_depth = glIsEnabled(GL_DEPTH_TEST);
    GLboolean prev_cull  = glIsEnabled(GL_CULL_FACE);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(g_gl.program);
    glUniform2f(g_gl.loc_screenSize, (float)w, (float)h);
    glBindBuffer(GL_ARRAY_BUFFER, g_gl.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 (GLsizeiptr)(verts.size() * sizeof(VertexGL)),
                 verts.data(), GL_STREAM_DRAW);

    glEnableVertexAttribArray(g_gl.attr_pos);
    glVertexAttribPointer(g_gl.attr_pos, 2, GL_FLOAT, GL_FALSE, sizeof(VertexGL),
        (const void*)offsetof(VertexGL, x));
    glEnableVertexAttribArray(g_gl.attr_color);
    glVertexAttribPointer(g_gl.attr_color, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VertexGL),
        (const void*)offsetof(VertexGL, r));

    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)verts.size());

    glDisableVertexAttribArray(g_gl.attr_pos);
    glDisableVertexAttribArray(g_gl.attr_color);

    // 復帰
    glBindBuffer(GL_ARRAY_BUFFER, prev_vbo);
    glUseProgram(prev_program);
    if (!prev_blend) glDisable(GL_BLEND);
    if (prev_depth)  glEnable(GL_DEPTH_TEST);
    if (prev_cull)   glEnable(GL_CULL_FACE);
}

// ============================================================================
// 仮想ジョイスティック attach
// ============================================================================

bool AttachVirtualJoystick() {
    SDL_InitSubSystem(SDL_INIT_JOYSTICK);

    SDL_VirtualJoystickDesc desc{};
    desc.version    = sizeof(desc);
    desc.type       = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes      = 0;
    desc.nbuttons   = kVirtualButtonCount;
    desc.nballs     = 0;
    desc.nhats      = 0;
    desc.vendor_id  = 0;
    desc.product_id = 0;
    desc.button_mask = 0u
        | (1u << SDL_GAMEPAD_BUTTON_SOUTH)
        | (1u << SDL_GAMEPAD_BUTTON_EAST)
        | (1u << SDL_GAMEPAD_BUTTON_WEST)
        | (1u << SDL_GAMEPAD_BUTTON_NORTH)
        | (1u << SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)
        | (1u << SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)
        | (1u << SDL_GAMEPAD_BUTTON_DPAD_UP)
        | (1u << SDL_GAMEPAD_BUTTON_DPAD_DOWN)
        | (1u << SDL_GAMEPAD_BUTTON_DPAD_LEFT)
        | (1u << SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    desc.axis_mask  = 0;
    desc.name       = "KrkrZ Virtual Pad";

    g_state->joystick_id = SDL_AttachVirtualJoystick(&desc);
    if (g_state->joystick_id == 0) return false;
    g_state->joystick = SDL_OpenJoystick(g_state->joystick_id);
    return g_state->joystick != nullptr;
}

} // anonymous namespace

void Init() {
    if (g_state) return;
    VPAD_LOG("Init() called");
    g_state = new State();
    bool attached = AttachVirtualJoystick();
    VPAD_LOG("AttachVirtualJoystick = %d (id=%u, nbuttons=%d)",
             attached, (unsigned)g_state->joystick_id, kVirtualButtonCount);
    InstallEventFilter();
    TVPRegisterPostRenderCallback(RenderSDL, nullptr);
    TVPRegisterPostRenderCallbackGL(RenderGL, nullptr);
    VPAD_LOG("post-render callbacks + event filter registered");
}

void Shutdown() {
    if (!g_state) return;
    TVPUnregisterPostRenderCallback(RenderSDL, nullptr);
    TVPUnregisterPostRenderCallbackGL(RenderGL, nullptr);
    UninstallEventFilter();
    ShutdownGL();
    if (g_state->joystick) {
        SDL_CloseJoystick(g_state->joystick);
        g_state->joystick = nullptr;
    }
    if (g_state->joystick_id) {
        SDL_DetachVirtualJoystick(g_state->joystick_id);
        g_state->joystick_id = 0;
    }
    delete g_state;
    g_state = nullptr;
}

void SetEnabled(bool enabled) { g_enabled.store(enabled); }

void SetOpacity(float opacity) {
    if (opacity < 0.0f) opacity = 0.0f;
    if (opacity > 1.0f) opacity = 1.0f;
    g_opacity.store(opacity);
}

void SetPreset(int preset) { g_preset.store(preset); }

} // namespace virtualpad
