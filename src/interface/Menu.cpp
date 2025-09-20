#include "../precompiled.h"
#include "Menu.hpp"

#ifdef _WIN32
#include <Windows.h>
#elif defined(__APPLE__)
#include <ApplicationServices/ApplicationServices.h>
#elif defined(__linux__) && !defined(__ANDROID__)
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#endif

static bool IsCapsLockOn() {
#ifdef _WIN32
    // Windows: VK_CAPITAL toggle bit
    return (GetKeyState(VK_CAPITAL) & 0x1) != 0;

#elif defined(__APPLE__)
    // macOS: query global HID flags (no Objective-C++ required)
    CGEventFlags flags = CGEventSourceFlagsState(kCGEventSourceStateHIDSystemState);
    return (flags & kCGEventFlagMaskAlphaShift) != 0;

#elif defined(__linux__) && !defined(__ANDROID__)
    // Linux/X11: use XKB indicator state. If no DISPLAY, return false.
    const char* display_name = std::getenv("DISPLAY");
    if (!display_name || !*display_name) return false;

    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) return false;

    unsigned int stateMask = 0;
    bool on = false;
    if (XkbGetIndicatorState(dpy, XkbUseCoreKbd, &stateMask) == Success) {
        // Typically bit 0 corresponds to Caps Lock
        on = (stateMask & 0x01u) != 0;
    }
    XCloseDisplay(dpy);
    return on;

#else
    // Fallback (e.g., pure Wayland without X11): no reliable global query available
    return false;
#endif
}


namespace {
    // One source of truth for the help lines
    static const char* kHelpLines[] = {
        "Edit options with LEFT/RIGHT",
        "Move camera: W S A D E Q",
        "Rotate camera: mouse",
        "Adjust power: UP/DOWN",
        "Rotate cue: LEFT/RIGHT",
        "Raise/lower cue: R/F",
        "Strike: SPACE",
        "Toggle lights: keys 0-9",
    };
    constexpr int kHelpCount = static_cast<int>(sizeof(kHelpLines) / sizeof(kHelpLines[0]));

    // Hover tracking + cursors
    static bool g_anyHoverThisFrame = false;
    static GLFWcursor* g_cursorHand = nullptr;
    static GLFWcursor* g_cursorArrow = nullptr;


    // Generic renderer for the help text; draws at any anchor/scale/alignment
    static void RenderHelpBlock(
        Menu& menu, float uAnchor, float vTop,
        Alignment align, float scale, float lineGap)
    {
        float v = vTop;
        for (int i = 0; i < kHelpCount; ++i) {
            menu.AddText(uAnchor, v, kHelpLines[i], scale, align, false);
            v -= lineGap;
        }
    }

    // Queue of typed Unicode codepoints this frame
    static std::u32string g_charQueue;

    // GLFW will call this for *text* input (after layout & modifiers)
    static void CharCallback(GLFWwindow*, unsigned int codepoint) {
        g_charQueue.push_back(static_cast<char32_t>(codepoint));
    }

    // Append a single Unicode codepoint to a UTF-8 string
    static void AppendUTF8(std::string& out, char32_t cp) {
        if (cp <= 0x7F) {
            out.push_back(static_cast<char>(cp));
        }
        else if (cp <= 0x7FF) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        else if (cp <= 0xFFFF) {
            // skip UTF-16 surrogate range
            if (cp >= 0xD800 && cp <= 0xDFFF) return;
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
        else if (cp <= 0x10FFFF) {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    // Count codepoints in a UTF-8 string (rough, tolerant)
    static int CountCodepoints(const std::string& s) {
        size_t i = 0; int count = 0;
        while (i < s.size()) {
            unsigned char c = static_cast<unsigned char>(s[i++]);
            if (c < 0x80) { ++count; continue; }
            if ((c & 0xE0) == 0xC0) { if (i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) ++i; ++count; }
            else if ((c & 0xF0) == 0xE0) { for (int k = 0; k < 2 && i < s.size(); ++k) if ((static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) ++i; ++count; }
            else if ((c & 0xF8) == 0xF0) { for (int k = 0; k < 3 && i < s.size(); ++k) if ((static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) ++i; ++count; }
            else { ++count; }
        }
        return count;
    }

    #define U8C(str) reinterpret_cast<const char*>(u8##str)

    // UTF-8 icons (make sure your source file is saved as UTF-8)
    static const char* ICON_SETTINGS = U8C("\u2699"); // ⚙
    static const char* ICON_INFO = U8C("\u2139"); // ℹ

    // Smooth modal animation state (0..1)
    static float g_helpAnim = 0.0f;
    static float g_qsAnim = 0.0f;
    static float g_uiAnim = 0.0f;

    // --- Animation helpers ---
    static inline float EaseOutCubic(float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return 1.0f - std::pow(1.0f - t, 3.0f);
    }
    static inline float EaseInCubic(float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * t;
    }

    // Critically-damped style "towards" using exact exponential step.
    // tau controls how "snappy" it is; smaller = faster.
    static inline float Towards(float cur, float target, float dt, float tauOpen, float tauClose) {
        const float tau = (target > cur) ? tauOpen : tauClose;      // close a bit faster
        const float k = 1.0f - std::exp(-static_cast<float>(dt) / tau);
        float out = cur + (target - cur) * k;
        if (std::fabs(out - target) < 0.006f) out = target;  // bigger snap zone = no end lag
        return out;
    }

}

// --------------------------------------------------------------------//

Menu::Menu(const int width, const int height) : width_(width), height_(height) {
    GLFWwindow* win = glfwGetCurrentContext();
    if (win) {
        // Receive Unicode characters for all layouts (Greek, etc.)
        glfwSetCharCallback(win, CharCallback);
    }
    selected_ = -1;
    last_mouse_ = GLFW_RELEASE;
}

void Menu::Update(const int width, const int height) { width_ = width; height_ = height; }

void Menu::beginFrameCaptureMouse()
{
    texts_.clear();
    GLFWwindow* w = glfwGetCurrentContext();
    double mx, my;
    glfwGetCursorPos(w, &mx, &my);
    int ww, wh;
    glfwGetWindowSize(w, &ww, &wh);
    mouse_x_ = mx;
    mouse_y_ = wh - my;
}

float Menu::estimateWidthPx(const std::string& s, float scale) const
{
    constexpr float kAvg = 0.62f; // rough glyph width
    const float base = static_cast<float>(Config::default_font_size);
    return static_cast<float>(s.size()) * base * kAvg * scale;
}
float Menu::estimateHeightPx(float scale) const
{
    const float base = static_cast<float>(Config::default_font_size);
    return base * 1.1f * scale;
}

bool Menu::button(float u, float v, const std::string& label, float scale,
    Alignment align, bool emphasize, float* out_w, float* out_h)
{
    // compute rect
    float px = u * width_;
    float py = v * height_;
    float w = estimateWidthPx(label, scale);
    float h = estimateHeightPx(scale);

    float x0 = px, x1 = px + w;
    if (align == Alignment::CENTER) { x0 = px - w * 0.5f; x1 = px + w * 0.5f; }
    else if (align == Alignment::RIGHT) { x0 = px - w; x1 = px; }

    float y0 = py - h * 0.5f;
    float y1 = py + h * 0.5f;

    // Hover test (stable; not affected by the visual scale)
    const bool hover = (mouse_x_ >= x0 && mouse_x_ <= x1 && mouse_y_ >= y0 && mouse_y_ <= y1);
    if (hover) g_anyHoverThisFrame = true;

    // Slight enlargement when hovered OR emphasized (selected)
    const float drawScale = (emphasize || hover) ? scale * 1.06f : scale;

    // draw
    AddText(u, v, label, drawScale, align, emphasize || hover);

    // CLICK: frame-wide edge (set in Draw), requires hover
    bool clicked = hover && mouse_edge_down_;

    if (out_w) *out_w = (x1 - x0);
    if (out_h) *out_h = (y1 - y0);
    return clicked;
}

bool Menu::ConsumeEditedNames(std::string& outP1, std::string& outP2) {
    if (!names_dirty_) return false;
    outP1 = p1_name_;
    outP2 = p2_name_;
    names_dirty_ = false;
    return true;
}

bool Menu::ConsumePlayClicked() { bool b = play_clicked_;  play_clicked_ = false; return b; }
bool Menu::ConsumeExitClicked() { bool b = exit_clicked_;  exit_clicked_ = false; return b; }
bool Menu::ConsumeResetClicked() { bool b = reset_clicked_; reset_clicked_ = false; return b; }


std::u32string Menu::char_buffer_{};

void Menu::InstallCharCallback(GLFWwindow* window) {
    glfwSetCharCallback(window, &Menu::CharCallbackThunk);
}

void Menu::CharCallbackThunk(GLFWwindow* /*wnd*/, unsigned int codepoint) {
    // Store the Unicode scalar as-is. IME delivers finalized characters here.
    if (codepoint >= 0x20) { // ignore control chars
        char_buffer_.push_back(static_cast<char32_t>(codepoint));
    }
}

void Menu::DrawMainMenu(bool modalOpen, int winW, int winH,
    float mouseX, float mouseY, int curEnter,
    int& prevEnter, int& selected)
{
    auto isEnterOn = [&](int logicalIndex)->bool {
        if (selected == -1) return false;
        const bool pressed = (curEnter == GLFW_PRESS && prevEnter == GLFW_RELEASE);
        return pressed && (selected == logicalIndex);
        };

    // --- Play (center) unchanged ---
    const bool playSelected = (selected == 0);
    if (!modalOpen) {
        bool playClicked = button(0.5f, 0.70f, "Play", Ui(1.05f), Alignment::CENTER, playSelected, nullptr, nullptr) || isEnterOn(0);
        if (playClicked) play_clicked_ = true;
    }

    // --- Bottom-left rows (Quick Setup, How to Play) with pixel spacing ---
    const float scaleQuick = Ui(0.85f);
    const float scaleHelp = Ui(0.90f);
    const float hQuick_px = estimateHeightPx(scaleQuick);
    const float hHelp_px = estimateHeightPx(scaleHelp);

    const float baseH_px = std::max(hQuick_px, hHelp_px);
    const float gap_px = baseH_px * 0.60f;          // nice breathing room
    const float margin_px = baseH_px * 1.20f;          // distance from bottom

    auto vpx = [&](float px) { return px / static_cast<float>(height_); };

    // centers from bottom upward
    const float helpV = vpx(margin_px + hHelp_px * 0.5f);
    const float quickV = vpx(margin_px + hHelp_px + gap_px + hQuick_px * 0.5f);

    // Quick Setup (opens modal)
    if (!settings_open_) {
        const bool qsSelected = (selected == 1);
        const char* dd = settings_open_ ? "v" : ">";
        bool qsClicked = !modalOpen &&
            button(0.10f, quickV, std::string(dd) + "  Quick Setup",
                scaleQuick, Alignment::LEFT, qsSelected, nullptr, nullptr);
        if (qsClicked || isEnterOn(1)) { settings_open_ = true; selected = 1; }
    }

    // How to Play (opens modal)
    if (!help_open_) {
        const bool helpSel = (selected == 2);
        bool helpClicked = !modalOpen &&
            button(0.10f, helpV, std::string(ICON_INFO) + "  How to Play",
                scaleHelp, Alignment::LEFT, helpSel, nullptr, nullptr);
        if (helpClicked || isEnterOn(2)) { help_open_ = true; selected = 2; }
    }
}


void Menu::DrawPauseMenu(bool modalOpen, int winW, int winH,
    float mouseX, float mouseY, int curEnter,
    int& prevEnter, int& selected)
{
    auto isEnterOn = [&](int i)->bool {
        if (selected == -1) return false;
        bool pressed = (curEnter == GLFW_PRESS && prevEnter == GLFW_RELEASE);
        return pressed && (selected == i);
        };

    if (!modalOpen) {
        // Top "Resume" stays around the same anchor
        const bool resumeSel = (selected == 0);
        bool resumeClicked = button(0.5f, 0.84f, "Resume game", Ui(1.05f),
            Alignment::CENTER, resumeSel, nullptr, nullptr) || isEnterOn(0);
        if (resumeClicked) play_clicked_ = true;

        // The rest: pixel-derived vertical step
        const float itemScale = Ui(0.90f);
        const float step_px = estimateHeightPx(itemScale) * 1.25f;
        auto vpx = [&](float px) { return px / static_cast<float>(height_); };

        const float startV = 0.64f;         // keep the block centered near where it used to be
        const float stepV = vpx(step_px);  // convert pixels to NDC v

        const bool resetSel = (selected == 1);
        if (button(0.5f, startV + 0 * stepV, "Reset game", itemScale, Alignment::CENTER, resetSel, nullptr, nullptr) || isEnterOn(1)) {
            reset_clicked_ = true;
            rename_gate_open_ = true;
        }

        const bool qsSel = (selected == 2);
        const char* dd = settings_open_ ? "v" : ">";
        if (button(0.5f, startV - 1 * stepV, std::string(dd) + "  Quick Setup", itemScale, Alignment::CENTER, qsSel, nullptr, nullptr) || isEnterOn(2)) {
            settings_open_ = true; selected = 2;
        }

        const bool helpSel = (selected == 3);
        if (button(0.5f, startV - 2 * stepV, std::string(ICON_INFO) + "  How to Play", itemScale, Alignment::CENTER, helpSel, nullptr, nullptr) || isEnterOn(3)) {
            help_open_ = true; selected = 3;
        }

        const bool exitSel = (selected == 4);
        if (button(0.5f, startV - 3 * stepV, "Exit game", itemScale, Alignment::CENTER, exitSel, nullptr, nullptr) || isEnterOn(4)) {
            exit_clicked_ = true;
        }
    }
}


void Menu::DrawQuickSetupModal(int winW, int winH, float /*mouseX*/, float /*mouseY*/, bool has_started)
{
    // Scales
    const float titleScale = Ui(0.95f);
    const float rowScale = Ui(0.80f);
    const float closeScale = Ui(0.80f);

    // Metrics (pixels)
    const float titleH = estimateHeightPx(titleScale);
    const float rowH = estimateHeightPx(rowScale);
    const float closeH = estimateHeightPx(closeScale);
    const float btnW = estimateWidthPx("<", rowScale); // "<" and ">" same width

    // Labels
    std::string sf = std::format("Strike force: {:.2f}", Config::power_coeff);
    std::string bf = std::format("Ball friction: {:.4f}", Config::linear_damping);

    // Label widths
    const float sfW = estimateWidthPx(sf, rowScale);
    const float bfW = estimateWidthPx(bf, rowScale);

    // Spacing
    const float hGap = rowH * 0.55f; // label↔button
    const float gapTitleToRows = rowH * 0.95f;
    const float rowStep = rowH * 1.20f; // center-to-center rows
    const float gapRowsToClose = rowH * 0.95f;

    // Row group widths
    const float row1W = sfW + hGap + btnW + hGap + btnW;
    const float row2W = bfW + hGap + btnW + hGap + btnW;

    // Total height (2 rows)
    const float listH = rowH + rowStep;
    const float totalH = titleH + gapTitleToRows + listH + gapRowsToClose + closeH;

    // Center vertically
    const float yBottom = 0.5f * (height_ - totalH);
    const float yTop = yBottom + totalH;

	// Animation easing (0 = closed, 1 = fully open)
    const float a = std::clamp(g_qsAnim, 0.0f, 1.0f);
    const bool opening = settings_open_;

    // Progress (0→1) used for scale: out-cubic on open, decelerating on close
    const float prog = opening ? EaseOutCubic(a) : (1.0f - EaseInCubic(1.0f - a));

    // Displacement (slide amount in NDC): (open) 1−EaseOutCubic(a), (close) EaseInCubic(1−a)
    auto Vslide = [&](float v, float dir, float offsetPx) {
        const float ndc = offsetPx / static_cast<float>(height_);
        const float disp = opening ? (1.0f - EaseOutCubic(a)) : EaseInCubic(1.0f - a);
        return v + dir * ndc * disp;
        };

    // Scale shrinks gently toward close
    auto S = [&](float s) { return s * (0.88f + 0.12f * prog); };

    const bool interactive = (settings_open_ && prog > 0.08f);

    // ---------- Title (TOP, centered) ----------
    const float titleCy = yTop - 0.5f * titleH;
    AddText(0.5f, Vslide(titleCy / height_, -1.0f, 80.0f), "Quick Setup  v", S(titleScale), Alignment::CENTER, true);

    // Row centers (go DOWN from title)
    const float row1Cy = yTop - titleH - gapTitleToRows - 0.5f * rowH;
    const float row2Cy = row1Cy - rowStep;

    // Row 1 (centered horizontally as a group)
    const float row1Left = 0.5f * width_ - 0.5f * row1W;
    const float sfCx = row1Left + sfW * 0.5f;
    const float sfLeftCx = row1Left + sfW + hGap + btnW * 0.5f;
    const float sfRightCx = sfLeftCx + btnW + hGap;

    // Rows: slide gently up
    AddText(sfCx / width_, Vslide(row1Cy / height_, -0.5f, 60.0f), sf, S(rowScale), Alignment::CENTER, true);
    if (interactive && button(sfLeftCx / width_, Vslide(row1Cy / height_, -0.5f, 60.0f), "<", S(rowScale), Alignment::CENTER, false, nullptr, nullptr))
        Config::power_coeff -= 0.05f;
    if (interactive && button(sfRightCx / width_, Vslide(row1Cy / height_, -0.5f, 60.0f), ">", S(rowScale), Alignment::CENTER, false, nullptr, nullptr))
        Config::power_coeff += 0.05f;

    // Row 2 (centered horizontally as a group)
    const float row2Left = 0.5f * width_ - 0.5f * row2W;
    const float bfCx = row2Left + bfW * 0.5f;
    const float bfLeftCx = row2Left + bfW + hGap + btnW * 0.5f;
    const float bfRightCx = bfLeftCx + btnW + hGap;

    AddText(bfCx / width_, Vslide(row2Cy / height_, -0.5f, 60.0f), bf, S(rowScale), Alignment::CENTER, true);
    if (interactive && button(bfLeftCx / width_, Vslide(row2Cy / height_, -0.5f, 60.0f), "<", S(rowScale), Alignment::CENTER, false, nullptr, nullptr)) {
        Config::linear_damping = std::max(0.940f, Config::linear_damping - 0.0005f);
        Config::velocity_multiplier = Config::linear_damping;
    }
    if (interactive && button(bfRightCx / width_, Vslide(row2Cy / height_, -0.5f, 60.0f), ">", S(rowScale), Alignment::CENTER, false, nullptr, nullptr)) {
        Config::linear_damping = std::min(0.9995f, Config::linear_damping + 0.0005f);
        Config::velocity_multiplier = Config::linear_damping;
    }

    // Keyboard focus (0 = strike force, 1 = friction)
    static int focus = 0;
    GLFWwindow* w = glfwGetCurrentContext();
    static int prevUp = GLFW_RELEASE, prevDown = GLFW_RELEASE, prevLeft = GLFW_RELEASE, prevRight = GLFW_RELEASE, prevEsc = GLFW_RELEASE;
    const int kUp = glfwGetKey(w, GLFW_KEY_UP);
    const int kDown = glfwGetKey(w, GLFW_KEY_DOWN);
    const int kLeft = glfwGetKey(w, GLFW_KEY_LEFT);
    const int kRight = glfwGetKey(w, GLFW_KEY_RIGHT);
    const int kEsc = glfwGetKey(w, GLFW_KEY_ESCAPE);

    const bool upEdge = (kUp == GLFW_PRESS && prevUp == GLFW_RELEASE);
    const bool downEdge = (kDown == GLFW_PRESS && prevDown == GLFW_RELEASE);
    const bool leftEdge = (kLeft == GLFW_PRESS && prevLeft == GLFW_RELEASE);
    const bool rightEdge = (kRight == GLFW_PRESS && prevRight == GLFW_RELEASE);


    if (interactive) {
        if (upEdge)   focus = 0;
        if (downEdge) focus = 1;
    }
    if (focus == 0) {
        if (leftEdge) Config::power_coeff -= 0.05f;
		if (rightEdge) Config::power_coeff += 0.05f;
    } else {
        if (leftEdge) {
            Config::linear_damping = std::max(0.940f, Config::linear_damping - 0.0005f);
            Config::velocity_multiplier = Config::linear_damping;
        }
        if (rightEdge) {
            Config::linear_damping = std::min(0.9995f, Config::linear_damping + 0.0005f);
            Config::velocity_multiplier = Config::linear_damping;
        }
	}

    // Close (BOTTOM)
    const float closeCy = yBottom + 0.5f * closeH;
    if (interactive && button(0.5f, Vslide(closeCy / height_, +1.0f, 70.0f), "Close [Esc]", S(closeScale), Alignment::CENTER, false, nullptr, nullptr)) {
        settings_open_ = false;
        selected_ = has_started ? 2 : 1;
    }

    if (kEsc == GLFW_PRESS && prevEsc == GLFW_RELEASE) {
        settings_open_ = false;
        selected_ = has_started ? 2 : 1;
    }

    // Prev key states
    prevUp = kUp; prevDown = kDown; prevLeft = kLeft; prevRight = kRight; prevEsc = kEsc;
}


void Menu::DrawHelpModal(bool has_started)
{
    const float titleScale = Ui(1.1f);
    const float lineScale = Ui(0.75f);
    const float closeScale = Ui(0.8f);

    const float titleH = estimateHeightPx(titleScale);
    const float lineH = estimateHeightPx(lineScale);
    const float closeH = estimateHeightPx(closeScale);

    const float gapTitleToList = lineH * 0.95f;
    const float listStep = lineH * 1.10f;
    const float gapListToClose = lineH * 0.95f;

    const float listH = lineH + (kHelpCount - 1) * listStep;
    const float totalH = titleH + gapTitleToList + listH + gapListToClose + closeH;

    const float yBottom = 0.5f * (height_ - totalH);
    const float yTop = yBottom + totalH;

    const float a = std::clamp(g_helpAnim, 0.0f, 1.0f);
    const bool opening = help_open_;
    const float prog = opening ? EaseOutCubic(a) : (1.0f - EaseInCubic(1.0f - a));

    auto Vslide = [&](float v, float dir, float offsetPx) {
        const float ndc = offsetPx / static_cast<float>(height_);
        const float disp = opening ? (1.0f - EaseOutCubic(a)) : EaseInCubic(1.0f - a);
        return v + dir * ndc * disp;
        };

    auto S = [&](float s) { return s * (0.88f + 0.12f * prog); };

    const bool interactive = (help_open_ && prog > 0.08f);


    const float titleCy = yTop - 0.5f * titleH;
    // Title up
    AddText(0.5f, Vslide(titleCy / height_, -1.0f, 80.0f),
        std::string(ICON_INFO) + "  How to Play", S(titleScale), Alignment::CENTER, true);


    float y = yTop - titleH - gapTitleToList - 0.5f * lineH;
    for (int i = 0; i < kHelpCount; ++i) {
        AddText(0.5f, Vslide(y / height_, -0.5f, 60.0f), kHelpLines[i], S(lineScale), Alignment::CENTER, false);
        y -= listStep;
    }

    const float closeCy = yBottom + 0.5f * closeH;
    if (interactive && button(0.5f, Vslide(closeCy / height_, +1.0f, 70.0f), "Close [Esc]",
        S(closeScale), Alignment::CENTER, false, nullptr, nullptr)) {
        help_open_ = false;
        selected_ = has_started ? 3 : 2;
    }

    static int prevEsc = GLFW_RELEASE;
    int kEsc = glfwGetKey(glfwGetCurrentContext(), GLFW_KEY_ESCAPE);
    if (interactive && kEsc == GLFW_PRESS && prevEsc == GLFW_RELEASE) {
        help_open_ = false;
        selected_ = has_started ? 3 : 2;
    }
    prevEsc = kEsc;
}


bool Menu::DrawSettingsIcon(int /*winW*/, int /*winH*/, bool selected)
{
    const float iconScale = Ui(1.1f);
    const float iconH_px = estimateHeightPx(iconScale);
    auto vpx = [&](float px) { return px / static_cast<float>(height_); };

    // a bit above the bottom
    const float v = vpx(iconH_px * 1.1f);

    // return true on mouse click; highlight when selected by keyboard
    return button(0.97f, v, ICON_SETTINGS, iconScale, Alignment::RIGHT, selected, nullptr, nullptr);
}



void Menu::DrawUiSettingsModal(bool has_started)
{
    // --- spacing helpers ---
    auto vpx = [&](float px) { return px / static_cast<float>(height_); };

    const float titleScale = Ui(1.10f);
    const float rowScale = Ui(0.95f);
    const float cbScale = Ui(0.90f);

    const float lineH_px = estimateHeightPx(rowScale);
    const float gap_px = lineH_px * 1.25f;

    const float headV = 0.70f;
    const float row1V = headV - vpx(gap_px);                 // UI scale row
    const float guideV = row1V - vpx(gap_px);                 // guideline row
    const float row2V = guideV - vpx(gap_px * 1.20f);        // Player 1
    const float row3V = row2V - vpx(gap_px);                 // Player 2
    const float hintV = row3V - vpx(gap_px * 0.90f);
    const float closeV = hintV - vpx(gap_px);                 // Close


    // --- animation
    const float a = std::clamp(g_uiAnim, 0.0f, 1.0f);
    const bool opening = ui_settings_open_;
    const float prog = opening ? EaseOutCubic(a) : (1.0f - EaseInCubic(1.0f - a));

    auto Vslide = [&](float v, float dir, float offsetPx) {
        const float ndc = offsetPx / static_cast<float>(height_);
        const float disp = opening ? (1.0f - EaseOutCubic(a)) : EaseInCubic(1.0f - a);
        return v + dir * ndc * disp;
        };

    auto S = [&](float s) { return s * (0.88f + 0.12f * prog); };

    const bool interactive = (ui_settings_open_ && prog > 0.08f);



    // Heading up
    AddText(0.5f, Vslide(headV, -1.0f, 80.0f), std::string(ICON_SETTINGS) + "  Settings",
        S(titleScale), Alignment::CENTER, true);


    // -------------------------------
    // Keyboard focus & key edges
    // 0 = UI scale, 1 = Guideline, 2 = Player1, 3 = Player2, 4 = Close
    // -------------------------------
    static int uiFocus = 0;
    GLFWwindow* w = glfwGetCurrentContext();

    static int prevUp = GLFW_RELEASE, prevDown = GLFW_RELEASE, prevLeft = GLFW_RELEASE, prevRight = GLFW_RELEASE,
        prevEnter = GLFW_RELEASE, prevTab = GLFW_RELEASE, prevEsc = GLFW_RELEASE;
    const int kUp = glfwGetKey(w, GLFW_KEY_UP);
    const int kDown = glfwGetKey(w, GLFW_KEY_DOWN);
    const int kLeft = glfwGetKey(w, GLFW_KEY_LEFT);
    const int kRight = glfwGetKey(w, GLFW_KEY_RIGHT);
    const int kEnter = glfwGetKey(w, GLFW_KEY_ENTER);
    const int kEnter2 = glfwGetKey(w, GLFW_KEY_KP_ENTER);
    const int kTab = glfwGetKey(w, GLFW_KEY_TAB);
    const int kEsc = glfwGetKey(w, GLFW_KEY_ESCAPE);

    const bool upEdge = (kUp == GLFW_PRESS && prevUp == GLFW_RELEASE);
    const bool downEdge = (kDown == GLFW_PRESS && prevDown == GLFW_RELEASE);
    const bool leftEdge = (kLeft == GLFW_PRESS && prevLeft == GLFW_RELEASE);
    const bool rightEdge = (kRight == GLFW_PRESS && prevRight == GLFW_RELEASE);
    const bool enterEdge = ((kEnter == GLFW_PRESS || kEnter2 == GLFW_PRESS) && prevEnter == GLFW_RELEASE);
    const bool tabEdge = (kTab == GLFW_PRESS && prevTab == GLFW_RELEASE);

    auto wrap = [](int v, int lo, int hi) { int n = hi - lo + 1; v = (v - lo + n) % n; return lo + v; };

    // --- gating for name edits (only editable before game start or after Reset) ---
    const bool canEditNames = (!has_started) || rename_gate_open_;

    // -------------------------------
    // UI Scale row (50/75/100)
    // -------------------------------
    struct Opt { const char* txt; float val; };
    static constexpr Opt kOpts[] = { {"50%",0.50f}, {"75%",0.75f}, {"100%",1.00f} };
    static constexpr int kOptCount = 3;

    const float colGapPx = lineH_px * 0.60f;

    std::string labels[kOptCount];
    float widths[kOptCount] = { 0,0,0 };
    float totalPx = 0.0f;

    for (int i = 0; i < kOptCount; ++i) {
        const bool on = std::fabs(ui_scale_ - kOpts[i].val) < 0.001f;
        labels[i] = std::string(on ? "[x] " : "[ ] ") + kOpts[i].txt;
        widths[i] = estimateWidthPx(labels[i], cbScale);
        totalPx += widths[i];
    }
    totalPx += colGapPx * (kOptCount - 1);

    float x = 0.5f * width_ - 0.5f * totalPx;
    for (int i = 0; i < kOptCount; ++i) {
        const bool on = std::fabs(ui_scale_ - kOpts[i].val) < 0.001f;
        const float centerU = (x + widths[i] * 0.5f) / static_cast<float>(width_);
        if (button(centerU, Vslide(row1V, -0.5f, 60.0f), labels[i], S(cbScale), Alignment::CENTER, (on || uiFocus == 0), nullptr, nullptr)) {
            ui_scale_ = kOpts[i].val;
        }
        x += widths[i] + colGapPx;
    }

    // Keyboard cycle UI scale when focused on row 0 (not editing)
    if (interactive && active_input_ == -1 && uiFocus == 0) {
        auto idxFromScale = [&]() {
            int best = 0; float bd = 1e9f;
            for (int i = 0; i < kOptCount; ++i) { float d = std::fabs(ui_scale_ - kOpts[i].val); if (d < bd) { bd = d; best = i; } }
            return best;
            };
        int idx = idxFromScale();
        if (leftEdge) { idx = std::max(0, idx - 1); ui_scale_ = kOpts[idx].val; }
        if (rightEdge) { idx = std::min(kOptCount - 1, idx + 1); ui_scale_ = kOpts[idx].val; }
    }

    // -------------------------------
    // Guideline toggle
    // -------------------------------
    const std::string gLabel = std::string(show_guideline_ ? "[x] " : "[ ] ") + "Guideline";
    // Guideline row
    if (interactive && button(0.5f, Vslide(guideV, -0.5f, 60.0f), gLabel, S(Ui(0.90f)),
        Alignment::CENTER, (uiFocus == 1), nullptr, nullptr)) {
        show_guideline_ = !show_guideline_;
    }
    if (interactive && active_input_ == -1 && uiFocus == 1 && (leftEdge || rightEdge || enterEdge)) {
        show_guideline_ = !show_guideline_;
    }

    // -------------------------------
    // Player names (with caret & mouse focus)
    // -------------------------------
    auto nameRow = [&](float v, const char* label, int fieldIndex, std::string& target,
        float slideDir, float slidePx) -> bool
        {
            const float baseScale = Ui(0.95f);        // stable metrics for hit-testing
            const float drawScale = S(baseScale);     // animated visual scale
            const float drawV = Vslide(v, slideDir, slidePx); // animated vertical position

            std::string field = target;
            if (active_input_ == fieldIndex && canEditNames) {
                const bool caretOn = std::fmod(glfwGetTime(), 1.0) < 0.5;
                field += caretOn ? "|" : " ";
            }

            // Highlight either when editing or when row is focused (2 or 3)
            const bool rowFocused = (uiFocus == (fieldIndex == 0 ? 2 : 3));

            // Draw at the animated position/scale
            AddText(0.5f, drawV, std::string(label) + field, drawScale, Alignment::CENTER,
                (active_input_ == fieldIndex) || rowFocused);

            if (!canEditNames || !interactive) return false;

            // --- Hit test (stable size; animated position) ---
            const float labelW = estimateWidthPx(label, baseScale);
            const float fieldW = std::max(240.0f * ui_scale_,
                estimateWidthPx("MMMMMMMMMMMMMMMM", baseScale)); // min field width

            const float pxMid = 0.5f * width_;
            const float x0 = pxMid - (labelW + fieldW) * 0.5f + labelW;
            const float x1 = x0 + fieldW;

            const float y = drawV * height_;                 // use animated vertical position
            const float h = estimateHeightPx(baseScale);     // stable hitbox height
            const float y0 = y - h * 0.5f, y1 = y + h * 0.5f;
            const bool inside = (mouse_x_ >= x0 && mouse_x_ <= x1 && mouse_y_ >= y0 && mouse_y_ <= y1);

            if (mouse_edge_down_ && inside) {
                active_input_ = fieldIndex;   // focus this field
            }

            return mouse_edge_down_ && inside; // report if this click was inside this field
        };


    // Name rows (wrap the v passed to nameRow)
    nameRow(row2V, "Player 1: ", 0, p1_name_, -0.5f, 60.0f);
    nameRow(row3V, "Player 2: ", 1, p2_name_, -0.5f, 60.0f);

    bool clickedP1 = nameRow(row2V, "Player 1: ", 0, p1_name_, -0.5f, 60.0f);
    bool clickedP2 = nameRow(row3V, "Player 2: ", 1, p2_name_, -0.5f, 60.0f);

    // If the user clicked and it wasn't in either field, cancel editing.
    if (interactive && canEditNames && mouse_edge_down_ && !clickedP1 && !clickedP2) {
        active_input_ = -1;
    }


    // Start editing on Enter/Tab if focused on a name row and not already editing
    if (interactive && canEditNames && active_input_ == -1) {
        if (uiFocus == 2 && (enterEdge || tabEdge)) active_input_ = 0;
        if (uiFocus == 3 && (enterEdge || tabEdge)) active_input_ = 1;
    }

    // -------------------------------
    // Name typing handler (only when a field is active)
    // - continuous Backspace
    // - Enter/Tab advances P1 -> P2 -> Close
    // - Esc exits the field
    // -------------------------------
    if (interactive && canEditNames && (active_input_ == 0 || active_input_ == 1)) {
        auto& target = (active_input_ == 0 ? p1_name_ : p2_name_);
        const int maxCodepoints = 18;

        // helper to erase last UTF-8 codepoint
        auto eraseLast = [&]() {
            if (!target.empty()) {
                size_t i = target.size();
                do { --i; } while (i > 0 && (static_cast<unsigned char>(target[i]) & 0xC0) == 0x80);
                target.erase(i);
                names_dirty_ = true;
            }
            };

        // Backspace auto-repeat
        static bool   bsWasDown = false;
        static double bsNext = 0.0;
        bool   bsDown = (glfwGetKey(w, GLFW_KEY_BACKSPACE) == GLFW_PRESS);
        double now = glfwGetTime();
        if (bsDown) {
            if (!bsWasDown) { eraseLast(); bsNext = now + 0.45; }   // initial delay
            else if (now >= bsNext) { eraseLast(); bsNext = now + 0.04; } // repeat rate
        }
        bsWasDown = bsDown;

        // Feed text from CharCallback, ignore newline/tab
        for (char32_t cp : g_charQueue) {
            if (cp == U'\r' || cp == U'\n' || cp == U'\t') continue;
            if (CountCodepoints(target) < maxCodepoints) {
                AppendUTF8(target, cp);
                names_dirty_ = true;
            }
        }
        g_charQueue.clear();

        // Enter/Tab -> next field or finish
        bool enterNow = (kEnter == GLFW_PRESS || kEnter2 == GLFW_PRESS);
        if ((enterNow && prevEnter == GLFW_RELEASE) || tabEdge) {
            if (active_input_ == 0) {         // go to P2
                active_input_ = 1;
                uiFocus = 3;
            }
            else {                           // leave after P2
                active_input_ = -1;
                uiFocus = 4;                   // move to Close
            }
        }

        // Esc exits the current field (doesn't close modal)
        if (kEsc == GLFW_PRESS && prevEsc == GLFW_RELEASE) {
            active_input_ = -1;
        }
    }

    // -------------------------------
    // Hint + Close
    // -------------------------------
    if (has_started && !rename_gate_open_) {
        AddText(0.5f, Vslide(hintV, -0.5f, 55.0f), "Names are editable after Reset game.", S(Ui(0.75f)),
            Alignment::CENTER, false);
    }

    // Close down
    if (interactive && button(0.5f, Vslide(closeV, +1.0f, 70.0f), "Close [Esc]", S(Ui(0.85f)),
        Alignment::CENTER, (uiFocus == 4), nullptr, nullptr)) {
        ui_settings_open_ = false;
        active_input_ = -1;
        rename_gate_open_ = false;
    }

    // -------------------------------
    // Global Esc:
    // - if editing a field → exit field
    // - else → close modal
    // -------------------------------
    if (interactive && kEsc == GLFW_PRESS && prevEsc == GLFW_RELEASE) {
        if (active_input_ != -1) {
            active_input_ = -1;
        }
        else {
            ui_settings_open_ = false;
            rename_gate_open_ = false;
        }
    }

    // -------------------------------
    // Arrow navigation between rows (only when NOT editing)
    // -------------------------------
    if (interactive && active_input_ == -1) {
        if (upEdge)   uiFocus = wrap(uiFocus - 1, 0, 4);
        if (downEdge) uiFocus = wrap(uiFocus + 1, 0, 4);

        // Enter on "Close" closes
        if (uiFocus == 4 && enterEdge) {
            ui_settings_open_ = false;
            rename_gate_open_ = false;
        }
    }

    // keep previous key states
    prevUp = kUp; prevDown = kDown; prevLeft = kLeft; prevRight = kRight;
    prevEnter = (kEnter == GLFW_PRESS || kEnter2 == GLFW_PRESS) ? GLFW_PRESS : GLFW_RELEASE;
    prevTab = kTab; prevEsc = kEsc;
}


void Menu::Draw(const bool not_loaded, const bool has_started)
{
    texts_.clear();
    GLFWwindow* w = glfwGetCurrentContext();

    const int winW = width_, winH = height_;
    double mx, my; glfwGetCursorPos(w, &mx, &my);
    const float mouseX = static_cast<float>(mx);
    const float mouseY = static_cast<float>(winH - my);
    mouse_x_ = mouseX; mouse_y_ = mouseY;

    static int prevMouse = GLFW_RELEASE;
    const  int curMouse = glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT);
    static int prevEnter = GLFW_RELEASE;
    const  int curEnter = glfwGetKey(w, GLFW_KEY_ENTER);

    // NEW: edge-detect left/right for keyboard nav
    static int prevLeft = GLFW_RELEASE;
    static int prevRight = GLFW_RELEASE;
    const  int curLeft = glfwGetKey(w, GLFW_KEY_LEFT);
    const  int curRight = glfwGetKey(w, GLFW_KEY_RIGHT);
    const  bool leftEdge = (curLeft == GLFW_PRESS && prevLeft == GLFW_RELEASE);
    const  bool rightEdge = (curRight == GLFW_PRESS && prevRight == GLFW_RELEASE);

    mouse_edge_down_ = (curMouse == GLFW_PRESS && prevMouse == GLFW_RELEASE);


    // --- modal animation (open→1, closed→0) ---
    static double tPrev = 0.0;
    double tNow = glfwGetTime();
    if (tPrev == 0.0) tPrev = tNow;
    double dt = tNow - tPrev;
    tPrev = tNow;

    // Reset per-frame hover flag
    g_anyHoverThisFrame = false;

    // Slightly faster closing for a crisp finish (tweak to taste)
    constexpr float kTauOpen = 0.10f;  // ~100ms feel
    constexpr float kTauClose = 0.085f;  // ~75ms feel

    g_helpAnim = Towards(g_helpAnim, help_open_ ? 1.0f : 0.0f, static_cast<float>(dt), kTauOpen, kTauClose);
    g_qsAnim = Towards(g_qsAnim, settings_open_ ? 1.0f : 0.0f, static_cast<float>(dt), kTauOpen, kTauClose);
    g_uiAnim = Towards(g_uiAnim, ui_settings_open_ ? 1.0f : 0.0f, static_cast<float>(dt), kTauOpen, kTauClose);

    // Block background nav while any modal is animating
    const bool modalOpen =
        settings_open_ || help_open_ || ui_settings_open_ ||
        g_helpAnim > 0.001f || g_qsAnim > 0.001f || g_uiAnim > 0.001f;

    if (!modalOpen) ControlState(); // keeps up/down behavior

    // Title
    AddText(0.5f, 0.90f, "8 Ball Pool", Ui(2.0f), Alignment::CENTER);

    // -------- LEFT/RIGHT PAGE LOGIC --------
    // Main page indices:   0=Play (center), 1=Quick Setup (left), 2=How to Play (left), 3=Settings ⚙ (right)
    // Pause page indices:  0..4 center column (Resume/Reset/QuickSetup/HowTo/Exit), 5=Settings ⚙ (right)
    if (!modalOpen) {
        if (!has_started) {
            // clamp & init
            if (selected_ < 0) selected_ = 0;
            if (selected_ > 3) selected_ = 3;

            if (rightEdge) {
                if (selected_ == 0)        selected_ = 3;      // center -> settings
                else if (selected_ == 1 ||
                    selected_ == 2)    selected_ = 3;      // left block -> settings
                // if already 3, stay
            }
            if (leftEdge) {
                if (selected_ == 3)        selected_ = 1;      // settings -> left block (top)
                else if (selected_ == 0)   selected_ = 1;      // center -> left block
                // if 1 or 2, stay in left block
            }
        }
        else {
            // pause page
            if (selected_ < 0) selected_ = 0;
            if (selected_ > 5) selected_ = 5;

            // remember last focused center item to return to from ⚙
            static int lastCenterIdx = 0;
            if (selected_ >= 0 && selected_ <= 4) lastCenterIdx = selected_;

            if (rightEdge) {
                if (selected_ <= 4) selected_ = 5;             // any center -> settings
            }
            if (leftEdge) {
                if (selected_ == 5) selected_ = lastCenterIdx; // settings -> last center
            }
        }
    }
    // ---------------------------------------

    // Main vs Pause
    if (!has_started) DrawMainMenu(modalOpen, winW, winH, mouseX, mouseY, curEnter, prevEnter, selected_);
    else              DrawPauseMenu(modalOpen, winW, winH, mouseX, mouseY, curEnter, prevEnter, selected_);

    // Modals
    // Draw modals while animating, not just while *open*
    if (g_qsAnim > 0.001f) DrawQuickSetupModal(winW, winH, mouseX, mouseY, has_started);
    if (g_helpAnim > 0.001f) DrawHelpModal(has_started);
    if (g_uiAnim > 0.001f) DrawUiSettingsModal(has_started);

    // ⚙ Settings icon (keyboard-activatable)
    const int gearIndex = has_started ? 5 : 3; // index we gave to the gear on each page
    if (!ui_settings_open_) {
        const bool isSelected = (selected_ == gearIndex);
        const bool enterOnGear = isSelected && (curEnter == GLFW_PRESS && prevEnter == GLFW_RELEASE);
        const bool clicked = DrawSettingsIcon(winW, winH, isSelected);
        if (clicked || enterOnGear) {
            ui_settings_open_ = true;
            active_input_ = -1;
        }
    }

    prevMouse = curMouse;
    prevEnter = curEnter;
    prevLeft = curLeft;
    prevRight = curRight;

    // If no text field is focused, drop any typed characters this frame
    if (active_input_ == -1 && !g_charQueue.empty())
        g_charQueue.clear();

    // Set cursor shape based on hover
    if (GLFWwindow* cw = glfwGetCurrentContext()) {
        if (!g_cursorHand)  g_cursorHand = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
        if (!g_cursorArrow) g_cursorArrow = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
        glfwSetCursor(cw, g_anyHoverThisFrame ? g_cursorHand : g_cursorArrow);
    }
}


void Menu::ControlState()
{
    GLFWwindow* window = glfwGetCurrentContext();

    // DOWN: move selection down; if nothing selected yet, start at 0
    const int down_state = glfwGetKey(window, GLFW_KEY_DOWN);
    if (down_state == GLFW_PRESS && last_down_state_ != GLFW_PRESS) {
        if (selected_ == -1) selected_ = 0; else selected_++;
    }
    last_down_state_ = down_state;

    // UP: move selection up; if nothing selected yet, start at 0
    const int up_state = glfwGetKey(window, GLFW_KEY_UP);
    if (up_state == GLFW_PRESS && last_up_state_ != GLFW_PRESS) {
        if (selected_ == -1) selected_ = 0; else selected_--;
    }
    last_up_state_ = up_state;
}

void Menu::AddText(const float u, const float v, const std::string& text,
    const float scale, Alignment alignment, const bool selected)
{
    // multiply the per-item scale; TextRenderer already multiplies this into its font_scale_
    texts_.emplace_back(u * width_, v * height_, text, scale, alignment, selected);
}
