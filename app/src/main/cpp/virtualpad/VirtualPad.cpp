// VirtualPad プラグイン本体 - 状態 / イベント / 2 つの描画パス。
//
//   - SDL_Renderer 経路 (RenderSDL): KRKRZ_USE_OPENGL=OFF 時の SDLDrawDevice 用
//   - OpenGL ES 直接経路 (RenderGL): KRKRZ_USE_OPENGL=ON 時の SDLOGLDrawDevice 用
//
// プラグインは両方の post-render コールバックに登録するので、エンジン側がどちらの
// パスでビルドされていても同じ見た目で描画される。

#include "VirtualPad.h"

#include <SDL3/SDL.h>
#include <glad/gles2.h>   // KrkrZ エンジンと同じ glad 経由で GL 関数を解決する

#include <atomic>
#include <cstring>
#include <unordered_map>

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
// レイアウト + 状態 (両描画パスから共用)
// ============================================================================

// 仮想ジョイスティックの button index。SDL3 は SDL_VirtualJoystickDesc::button_mask
// で set したビット (SDL_GamepadButton enum 値) を **enum 順に詰めて** joystick button
// index 0,1,2... に割り当てる。なので button_mask に set した 6 ボタンが順に 0..5。
constexpr int kBtnIndexA          = 0;   // SOUTH  (1st set bit in mask)
constexpr int kBtnIndexB          = 1;   // EAST   (2nd)
constexpr int kBtnIndexDpadUp     = 2;   // DPAD_UP (3rd, mask bit 11)
constexpr int kBtnIndexDpadDown   = 3;   // DPAD_DOWN (4th, mask bit 12)
constexpr int kBtnIndexDpadLeft   = 4;   // DPAD_LEFT (5th, mask bit 13)
constexpr int kBtnIndexDpadRight  = 5;   // DPAD_RIGHT (6th, mask bit 14)
constexpr int kVirtualButtonCount = 6;

struct ButtonLayout {
    float x, y, w, h;    // 正規化座標 (window 内 0..1)
    int button_index;
};

// Preset 0: bottom layout (D-pad 左下、A/B 右下)
constexpr ButtonLayout kLayoutBottom[] = {
    { 0.04f, 0.55f, 0.10f, 0.10f, kBtnIndexDpadUp    },
    { 0.04f, 0.85f, 0.10f, 0.10f, kBtnIndexDpadDown  },
    { 0.00f, 0.70f, 0.08f, 0.10f, kBtnIndexDpadLeft  },
    { 0.12f, 0.70f, 0.08f, 0.10f, kBtnIndexDpadRight },
    { 0.86f, 0.78f, 0.12f, 0.15f, kBtnIndexA },
    { 0.74f, 0.85f, 0.10f, 0.10f, kBtnIndexB },
};
constexpr int kLayoutBottomCount = sizeof(kLayoutBottom) / sizeof(kLayoutBottom[0]);

struct State {
    std::atomic<bool>  enabled{true};
    std::atomic<float> opacity{0.55f};
    std::atomic<int>   preset{0};

    SDL_Joystick    *joystick = nullptr;
    SDL_JoystickID   joystick_id = 0;

    std::unordered_map<SDL_FingerID, int> finger_button;
    int button_press_count[kVirtualButtonCount] = {0};
};

State *g_state = nullptr;

const ButtonLayout *CurrentLayout(int *count_out) {
    *count_out = kLayoutBottomCount;
    return kLayoutBottom;
}

int HitTest(float nx, float ny) {
    int count;
    const ButtonLayout *layout = CurrentLayout(&count);
    for (int i = 0; i < count; ++i) {
        const ButtonLayout &b = layout[i];
        if (nx >= b.x && nx < b.x + b.w && ny >= b.y && ny < b.y + b.h) return i;
    }
    return -1;
}

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
// イベント (finger -> button)
// ============================================================================

bool SDLCALL EventWatcher(void *ud, SDL_Event *event) {
    if (!g_state || !g_state->enabled.load()) return true;
    if (!g_state->joystick) return true;

    int count;
    const ButtonLayout *layout = CurrentLayout(&count);

    switch (event->type) {
        case SDL_EVENT_FINGER_DOWN: {
            int idx = HitTest(event->tfinger.x, event->tfinger.y);
            if (idx >= 0) {
                g_state->finger_button[event->tfinger.fingerID] = idx;
                PressButton(layout[idx].button_index);
            }
            break;
        }
        case SDL_EVENT_FINGER_UP:
        case SDL_EVENT_FINGER_CANCELED: {
            auto it = g_state->finger_button.find(event->tfinger.fingerID);
            if (it != g_state->finger_button.end()) {
                ReleaseButton(layout[it->second].button_index);
                g_state->finger_button.erase(it);
            }
            break;
        }
        default:
            break;
    }
    return true;
}

// ============================================================================
// SDL_Renderer 経路の描画
// ============================================================================

void RenderSDL(SDL_Renderer *renderer, void *ud) {
    if (!g_state || !g_state->enabled.load()) return;
    if (!renderer) return;

    int w = 0, h = 0;
    SDL_GetCurrentRenderOutputSize(renderer, &w, &h);
    if (w <= 0 || h <= 0) return;

    int count;
    const ButtonLayout *layout = CurrentLayout(&count);
    Uint8 alpha = (Uint8)(g_state->opacity.load() * 255.0f);

    SDL_BlendMode old_blend = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(renderer, &old_blend);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < count; ++i) {
        const ButtonLayout &b = layout[i];
        SDL_FRect rect = { b.x * w, b.y * h, b.w * w, b.h * h };
        bool pressed = g_state->button_press_count[b.button_index] > 0;
        if (pressed) {
            SDL_SetRenderDrawColor(renderer, 255, 220, 80, alpha);
        } else {
            SDL_SetRenderDrawColor(renderer, 50, 50, 50, alpha);
        }
        SDL_RenderFillRect(renderer, &rect);
        SDL_SetRenderDrawColor(renderer, 220, 220, 220, alpha);
        SDL_RenderRect(renderer, &rect);
    }

    SDL_SetRenderDrawBlendMode(renderer, old_blend);
}

// ============================================================================
// OpenGL ES 経路の描画 (shader + VBO の lazy init、毎フレーム同じ resource を使用)
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

struct VertexGL {
    float    x, y;
    uint8_t  r, g, b, a;
};
static_assert(sizeof(VertexGL) == 12, "VertexGL packing");

void RenderGL(void *ud) {
    if (!g_state || !g_state->enabled.load()) return;
    if (!InitGL()) return;

    GLint vp[4] = {0};
    glGetIntegerv(GL_VIEWPORT, vp);
    int w = vp[2], h = vp[3];
    if (w <= 0 || h <= 0) return;

    // 既存の GL state を退避
    GLint  prev_program = 0;       glGetIntegerv(GL_CURRENT_PROGRAM, &prev_program);
    GLint  prev_vbo     = 0;       glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prev_vbo);
    GLboolean prev_blend = glIsEnabled(GL_BLEND);
    GLint  prev_blend_src = 0;     glGetIntegerv(GL_BLEND_SRC_ALPHA, &prev_blend_src);
    GLint  prev_blend_dst = 0;     glGetIntegerv(GL_BLEND_DST_ALPHA, &prev_blend_dst);
    GLboolean prev_depth = glIsEnabled(GL_DEPTH_TEST);
    GLboolean prev_cull  = glIsEnabled(GL_CULL_FACE);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    int count;
    const ButtonLayout *layout = CurrentLayout(&count);
    uint8_t alpha = (uint8_t)(g_state->opacity.load() * 255.0f);

    // 6 vertices per quad (2 triangles), 8 buttons = 48 vertices, ボタンは枠線も
    // 出すので fill + outline で 12 quads 程度。最初は fill のみ。
    VertexGL verts[kLayoutBottomCount * 6];
    int vcount = 0;
    for (int i = 0; i < count; ++i) {
        const ButtonLayout &b = layout[i];
        bool pressed = g_state->button_press_count[b.button_index] > 0;
        uint8_t r = pressed ? 255 : 50;
        uint8_t g = pressed ? 220 : 50;
        uint8_t bb = pressed ?  80 : 50;
        float x0 = b.x * w, y0 = b.y * h;
        float x1 = (b.x + b.w) * w, y1 = (b.y + b.h) * h;
        verts[vcount++] = { x0, y0, r, g, bb, alpha };
        verts[vcount++] = { x1, y0, r, g, bb, alpha };
        verts[vcount++] = { x1, y1, r, g, bb, alpha };
        verts[vcount++] = { x0, y0, r, g, bb, alpha };
        verts[vcount++] = { x1, y1, r, g, bb, alpha };
        verts[vcount++] = { x0, y1, r, g, bb, alpha };
    }

    glUseProgram(g_gl.program);
    glUniform2f(g_gl.loc_screenSize, (float)w, (float)h);
    glBindBuffer(GL_ARRAY_BUFFER, g_gl.vbo);
    glBufferData(GL_ARRAY_BUFFER, vcount * (GLsizeiptr)sizeof(VertexGL), verts, GL_STREAM_DRAW);

    glEnableVertexAttribArray(g_gl.attr_pos);
    glVertexAttribPointer(g_gl.attr_pos, 2, GL_FLOAT, GL_FALSE, sizeof(VertexGL),
        (const void*)offsetof(VertexGL, x));
    glEnableVertexAttribArray(g_gl.attr_color);
    glVertexAttribPointer(g_gl.attr_color, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VertexGL),
        (const void*)offsetof(VertexGL, r));

    glDrawArrays(GL_TRIANGLES, 0, vcount);

    glDisableVertexAttribArray(g_gl.attr_pos);
    glDisableVertexAttribArray(g_gl.attr_color);

    // GL state を復帰 (他の描画コードが干渉しないように)
    glBindBuffer(GL_ARRAY_BUFFER, prev_vbo);
    glUseProgram(prev_program);
    if (!prev_blend) glDisable(GL_BLEND);
    if (prev_depth)  glEnable(GL_DEPTH_TEST);
    if (prev_cull)   glEnable(GL_CULL_FACE);
}

// ============================================================================
// SDL_AttachVirtualJoystick
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
    VPAD_LOG("AttachVirtualJoystick = %d (id=%u)", attached, (unsigned)g_state->joystick_id);
    SDL_AddEventWatch(EventWatcher, nullptr);
    TVPRegisterPostRenderCallback(RenderSDL, nullptr);
    TVPRegisterPostRenderCallbackGL(RenderGL, nullptr);
    VPAD_LOG("post-render callbacks registered (sdl=%p gl=%p)", (void*)&RenderSDL, (void*)&RenderGL);
}

void Shutdown() {
    if (!g_state) return;
    TVPUnregisterPostRenderCallback(RenderSDL, nullptr);
    TVPUnregisterPostRenderCallbackGL(RenderGL, nullptr);
    SDL_RemoveEventWatch(EventWatcher, nullptr);
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

void SetEnabled(bool enabled) {
    if (g_state) g_state->enabled.store(enabled);
}

void SetOpacity(float opacity) {
    if (opacity < 0.0f) opacity = 0.0f;
    if (opacity > 1.0f) opacity = 1.0f;
    if (g_state) g_state->opacity.store(opacity);
}

void SetPreset(int preset) {
    if (g_state) g_state->preset.store(preset);
}

} // namespace virtualpad
