// aimlab - Win32 + OpenGL + Dear ImGui control panel for the aimbot.

// ===== Discord invite link =====
// Edit this constant to point to your Discord server. The "DiscordServer"
// label in the title row opens this URL in the user's default browser.
static constexpr const char *kDiscordUrl = "https://discord.gg/your-server";

#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>
#include <windowsx.h>

#include <GL/gl.h>
#include <GL/wglext.h>

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"

#include "core/aim_worker.h"
#include "capture/color_sample.h"
#include "config.h"
#include "input/hotkey.h"
#include "i18n.h"
#include "ui/logo.h"
#include "input/mouse_input.h"
#include "capture/screen_capture.h"
#include "ui/window_picker.h"

#pragma comment(lib, "opengl32.lib")

// ---------- Forward declarations ----------
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// ---------- Globals (single-instance) ----------
static aimlab::ConfigStore *g_cfg = nullptr;
static aimlab::AimWorker *g_worker = nullptr;
static constexpr int kHotkeyId = 1;
static HWND g_hwnd = nullptr;
static ImFont *g_fontNormal = nullptr;
static ImFont *g_fontTitle = nullptr;
static LogoImage g_logo;
static HICON g_hIconSmall = nullptr;
static HICON g_hIconBig = nullptr;
static bool g_capturingHotkey = false;

// ---------- Window picker ----------
static bool g_showWindowPicker = false;
static std::vector<aimlab::WindowInfo> g_pickerCache;
static char g_pickerFilter[128] = "";
static bool g_pickerCacheDirty = true;

// ---------- Presets ----------
struct Preset
{
    std::string name; // owned so we can store user-saved names
    int targetR, targetG, targetB;
    int colorTolerance;
    int sampleRadiusPx;
    int minPixelNeighbors;
    int clusterRadiusPx;
    int minClusterSize;
    int aimMaxSpeed;
    int aimPower;
    int aimDeadzonePx;
    bool aimSnapClose;
    bool autoClickEnabled;
    int autoClickDistancePx;
    int inputMethod;
};

static std::vector<Preset> g_presets;

// Built-in presets are added first; user presets get appended after.
// Default: balanced everyday settings.
// Aggressive: max rage - 3x aim power, fastest speed, loosest detection.
// Conservative: tame - for slower, more careful play.
// Sniper: precision - slow, tight detection, snap disabled so it eases in.
static void InitBuiltinPresets()
{
    g_presets.push_back({"Default", 255, 0, 0,
                         /*tol*/ 25, /*rad*/ 15, /*nbr*/ 4,
                         /*clRad*/ 10, /*clSz*/ 20,
                         /*maxSpd*/ 40, /*power*/ 150, /*dead*/ 2, /*snap*/ true,
                         /*auto*/ true, /*autoDist*/ 30,
                         /*method*/ 0});

    g_presets.push_back({"Aggressive", 255, 0, 0,
                         /*tol*/ 80, /*rad*/ 20, /*nbr*/ 1,
                         /*clRad*/ 25, /*clSz*/ 5,
                         /*maxSpd*/ 200, /*power*/ 300, /*dead*/ 0, /*snap*/ true,
                         /*auto*/ true, /*autoDist*/ 120,
                         /*method*/ 0});

    g_presets.push_back({"Conservative", 255, 0, 0,
                         /*tol*/ 12, /*rad*/ 10, /*nbr*/ 6,
                         /*clRad*/ 8, /*clSz*/ 40,
                         /*maxSpd*/ 18, /*power*/ 100, /*dead*/ 3, /*snap*/ true,
                         /*auto*/ false, /*autoDist*/ 20,
                         /*method*/ 0});

    g_presets.push_back({"Sniper", 255, 0, 0,
                         /*tol*/ 6, /*rad*/ 8, /*nbr*/ 8,
                         /*clRad*/ 5, /*clSz*/ 60,
                         /*maxSpd*/ 8, /*power*/ 100, /*dead*/ 1, /*snap*/ false,
                         /*auto*/ false, /*autoDist*/ 10,
                         /*method*/ 0});
}

static void ApplyPreset(aimlab::ConfigStore &cfg, const Preset &p)
{
    cfg.setTargetColor(p.targetR, p.targetG, p.targetB);
    cfg.setColorTolerance(p.colorTolerance);
    cfg.setSampleRadius(p.sampleRadiusPx);
    cfg.setMinPixelNeighbors(p.minPixelNeighbors);
    cfg.setClusterRadius(p.clusterRadiusPx);
    cfg.setMinClusterSize(p.minClusterSize);
    cfg.setAimMaxSpeed(p.aimMaxSpeed);
    cfg.setAimPower(p.aimPower);
    cfg.setAimDeadzone(p.aimDeadzonePx);
    cfg.setAimSnapClose(p.aimSnapClose);
    cfg.setAutoClick(p.autoClickEnabled);
    cfg.setAutoClickDistance(p.autoClickDistancePx);
    cfg.setInputMethod(p.inputMethod);
}

static const char *kUserPresetsFile = "user_presets.dat";

// Format (one preset per line): "name|R|G|B|tol|radius|neighbors|clusterRadius|clusterSize|maxSpeed|power|deadzone|snap|auto|clickDist|method"
// Names may not contain '|' or '\n'.
static bool SaveUserPresets()
{
    std::ofstream f(kUserPresetsFile, std::ios::trunc);
    if (!f)
        return false;
    for (size_t i = 4; i < g_presets.size(); ++i)
    { // skip the 4 builtins
        const Preset &p = g_presets[i];
        f << p.name << '|'
          << p.targetR << '|' << p.targetG << '|' << p.targetB << '|'
          << p.colorTolerance << '|' << p.sampleRadiusPx << '|'
          << p.minPixelNeighbors << '|' << p.clusterRadiusPx << '|'
          << p.minClusterSize << '|' << p.aimMaxSpeed << '|'
          << p.aimPower << '|' << p.aimDeadzonePx << '|'
          << (p.aimSnapClose ? 1 : 0) << '|'
          << (p.autoClickEnabled ? 1 : 0) << '|'
          << p.autoClickDistancePx << '|' << p.inputMethod << '\n';
    }
    return true;
}

static void LoadUserPresets()
{
    std::ifstream f(kUserPresetsFile);
    if (!f)
        return;
    std::string line;
    while (std::getline(f, line))
    {
        if (line.empty())
            continue;
        std::vector<std::string> tok;
        std::string cur;
        for (char c : line)
        {
            if (c == '|')
            {
                tok.push_back(cur);
                cur.clear();
            }
            else
            {
                cur.push_back(c);
            }
        }
        tok.push_back(cur);
        if (tok.size() < 16)
            continue;
        Preset p;
        p.name = tok[0];
        p.targetR = std::stoi(tok[1]);
        p.targetG = std::stoi(tok[2]);
        p.targetB = std::stoi(tok[3]);
        p.colorTolerance = std::stoi(tok[4]);
        p.sampleRadiusPx = std::stoi(tok[5]);
        p.minPixelNeighbors = std::stoi(tok[6]);
        p.clusterRadiusPx = std::stoi(tok[7]);
        p.minClusterSize = std::stoi(tok[8]);
        p.aimMaxSpeed = std::stoi(tok[9]);
        p.aimPower = std::stoi(tok[10]);
        p.aimDeadzonePx = std::stoi(tok[11]);
        p.aimSnapClose = std::stoi(tok[12]) != 0;
        p.autoClickEnabled = std::stoi(tok[13]) != 0;
        p.autoClickDistancePx = std::stoi(tok[14]);
        p.inputMethod = std::stoi(tok[15]);
        g_presets.push_back(std::move(p));
    }
}

// ---------- Hotkey helpers ----------
static const char *VkToName(int vk)
{
    switch (vk)
    {
    case VK_PRIOR:
        return "PageUp";
    case VK_NEXT:
        return "PageDown";
    case VK_HOME:
        return "Home";
    case VK_END:
        return "End";
    case VK_INSERT:
        return "Insert";
    case VK_DELETE:
        return "Delete";
    case VK_SPACE:
        return "Space";
    case VK_TAB:
        return "Tab";
    case VK_RETURN:
        return "Enter";
    case VK_ESCAPE:
        return "Esc";
    case VK_SHIFT:
        return "Shift";
    case VK_CONTROL:
        return "Ctrl";
    case VK_MENU:
        return "Alt";
    case VK_LWIN:
    case VK_RWIN:
        return "Win";
    case VK_F1:
        return "F1";
    case VK_F2:
        return "F2";
    case VK_F3:
        return "F3";
    case VK_F4:
        return "F4";
    case VK_F5:
        return "F5";
    case VK_F6:
        return "F6";
    case VK_F7:
        return "F7";
    case VK_F8:
        return "F8";
    case VK_F9:
        return "F9";
    case VK_F10:
        return "F10";
    case VK_F11:
        return "F11";
    case VK_F12:
        return "F12";
    }
    // Letters and digits
    if (vk >= 'A' && vk <= 'Z')
    {
        static thread_local char buf[2] = {0, 0};
        buf[0] = (char)vk;
        return buf;
    }
    if (vk >= '0' && vk <= '9')
    {
        static thread_local char buf[2] = {0, 0};
        buf[0] = (char)vk;
        return buf;
    }
    return "Key";
}

static std::string HotkeyToString(int mod, int vk)
{
    std::string s;
    if (mod & MOD_CONTROL)
        s += "Ctrl + ";
    if (mod & MOD_ALT)
        s += "Alt + ";
    if (mod & MOD_SHIFT)
        s += "Shift + ";
    if (mod & MOD_WIN)
        s += "Win + ";
    s += VkToName(vk);
    return s;
}

static int GetCurrentModifiers()
{
    int m = 0;
    if (GetKeyState(VK_CONTROL) & 0x8000)
        m |= MOD_CONTROL;
    if (GetKeyState(VK_MENU) & 0x8000)
        m |= MOD_ALT;
    if (GetKeyState(VK_SHIFT) & 0x8000)
        m |= MOD_SHIFT;
    if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000)
        m |= MOD_WIN;
    return m;
}

// ---------- OpenGL helpers ----------
static HGLRC g_glrc = nullptr;

static void SetPixelFormatForHDC(HDC hdc)
{
    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;
    int pf = ChoosePixelFormat(hdc, &pfd);
    if (pf)
        ::SetPixelFormat(hdc, pf, &pfd);
}

static bool InitOpenGL(HWND hwnd)
{
    HDC hdc = GetDC(hwnd);
    if (!hdc)
        return false;
    SetPixelFormatForHDC(hdc);

    // Step 1: create a legacy context just to get wglCreateContextAttribsARB.
    HGLRC legacy = wglCreateContext(hdc);
    if (!legacy)
    {
        ReleaseDC(hwnd, hdc);
        return false;
    }
    if (!wglMakeCurrent(hdc, legacy))
    {
        wglDeleteContext(legacy);
        ReleaseDC(hwnd, hdc);
        return false;
    }

    typedef HGLRC(WINAPI * PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int *);
    PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB =
        reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(
            wglGetProcAddress("wglCreateContextAttribsARB"));

    HGLRC modern = nullptr;
    if (wglCreateContextAttribsARB)
    {
        const int attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
            WGL_CONTEXT_MINOR_VERSION_ARB, 0,
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            0};
        modern = wglCreateContextAttribsARB(hdc, 0, attribs);
    }

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(legacy);

    if (!modern)
    {
        // Fallback: legacy context is fine for ImGui with GLSL 120.
        modern = legacy;
        if (!wglMakeCurrent(hdc, modern))
        {
            ReleaseDC(hwnd, hdc);
            return false;
        }
    }
    else
    {
        ReleaseDC(hwnd, hdc);
        hdc = GetDC(hwnd);
        if (!wglMakeCurrent(hdc, modern))
        {
            return false;
        }
    }

    g_glrc = modern;
    return true;
}

static void ShutdownOpenGL()
{
    if (g_glrc)
    {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(g_glrc);
        g_glrc = nullptr;
    }
}

// ---------- GUI ----------
// ---------- Modern purple/black theme ----------
static const ImVec4 kPurple = ImVec4(0.62f, 0.35f, 1.00f, 1.0f);       // accent
static const ImVec4 kPurpleBright = ImVec4(0.78f, 0.55f, 1.00f, 1.0f); // hover
static const ImVec4 kPurpleDeep = ImVec4(0.40f, 0.18f, 0.85f, 1.0f);   // active
static const ImVec4 kBgDeep = ImVec4(0.055f, 0.038f, 0.090f, 0.97f);
static const ImVec4 kBgPanel = ImVec4(0.085f, 0.062f, 0.130f, 1.00f);
static const ImVec4 kBgInput = ImVec4(0.130f, 0.092f, 0.200f, 1.00f);
static const ImVec4 kBgInputHov = ImVec4(0.175f, 0.120f, 0.285f, 1.00f);
static const ImVec4 kBorder = ImVec4(0.320f, 0.180f, 0.520f, 0.45f);
static const ImVec4 kTextHi = ImVec4(0.970f, 0.955f, 0.995f, 1.0f);
static const ImVec4 kTextMid = ImVec4(0.760f, 0.700f, 0.860f, 1.0f);
static const ImVec4 kTextLo = ImVec4(0.520f, 0.450f, 0.660f, 1.0f);

static void ApplyDarkTheme()
{
    ImGui::StyleColorsDark();
    ImGuiStyle &s = ImGui::GetStyle();
    s.WindowRounding = 10.0f;
    s.ChildRounding = 8.0f;
    s.FrameRounding = 6.0f;
    s.GrabRounding = 6.0f;
    s.TabRounding = 6.0f;
    s.PopupRounding = 8.0f;
    s.ScrollbarRounding = 8.0f;
    s.WindowPadding = ImVec2(18, 16);
    s.FramePadding = ImVec2(10, 5);
    s.ItemSpacing = ImVec2(10, 8);
    s.ItemInnerSpacing = ImVec2(8, 4);
    s.ScrollbarSize = 12.0f;
    s.GrabMinSize = 8.0f;
    s.IndentSpacing = 18.0f;
    s.Alpha = 1.0f;

    ImVec4 *c = s.Colors;

    // Backgrounds
    c[ImGuiCol_WindowBg] = kBgDeep;
    c[ImGuiCol_ChildBg] = kBgPanel;
    c[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.07f, 0.16f, 0.95f);
    c[ImGuiCol_MenuBarBg] = kBgPanel;

    // Borders
    c[ImGuiCol_Border] = kBorder;
    c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

    // Text
    c[ImGuiCol_Text] = kTextHi;
    c[ImGuiCol_TextDisabled] = kTextLo;

    // Headers (collapsing headers, selection)
    c[ImGuiCol_Header] = ImVec4(0.45f, 0.22f, 0.85f, 0.40f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.55f, 0.30f, 0.95f, 0.60f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.50f, 0.25f, 0.90f, 0.85f);

    // Buttons
    c[ImGuiCol_Button] = ImVec4(0.35f, 0.18f, 0.70f, 0.85f);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.52f, 0.28f, 0.95f, 0.95f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.42f, 0.20f, 0.82f, 1.00f);

    // Frames (inputs, drag ints, sliders background)
    c[ImGuiCol_FrameBg] = kBgInput;
    c[ImGuiCol_FrameBgHovered] = kBgInputHov;
    c[ImGuiCol_FrameBgActive] = ImVec4(0.22f, 0.14f, 0.34f, 1.0f);

    // Slider
    c[ImGuiCol_SliderGrab] = kPurple;
    c[ImGuiCol_SliderGrabActive] = kPurpleBright;

    // Checkmark
    c[ImGuiCol_CheckMark] = kPurpleBright;

    // Title
    c[ImGuiCol_TitleBg] = kBgDeep;
    c[ImGuiCol_TitleBgActive] = kBgPanel;

    // Scrollbar
    c[ImGuiCol_ScrollbarBg] = ImVec4(0.06f, 0.04f, 0.10f, 0.50f);
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.20f, 0.50f, 0.85f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.45f, 0.30f, 0.70f, 0.90f);
    c[ImGuiCol_ScrollbarGrabActive] = kPurple;

    // Separator
    c[ImGuiCol_Separator] = kBorder;
    c[ImGuiCol_SeparatorHovered] = ImVec4(0.55f, 0.30f, 0.95f, 0.70f);
    c[ImGuiCol_SeparatorActive] = kPurpleBright;
}

// Latest auto-sized ImGui window size (read after End() where GetWindowSize
// is no longer valid).
static ImVec2 g_lastImguiSize = ImVec2(900, 800);

// ---------- Drawing helpers ----------
static void DrawGlowPill(const ImVec2 &center, float radius, ImU32 col, int layers = 4)
{
    ImDrawList *dl = ImGui::GetWindowDrawList();
    for (int i = layers; i >= 1; --i)
    {
        float a = (0.10f / layers) * i;
        ImU32 c = (col & 0x00FFFFFF) | ((ImU32)(a * 255.0f) << 24);
        dl->AddCircleFilled(center, radius + i * 3.0f, c, 32);
    }
}

static void DrawSectionHeader(const char *label)
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList *dl = ImGui::GetWindowDrawList();
    ImU32 accent = IM_COL32(160, 95, 255, 220);
    // Purple bar on the left
    dl->AddRectFilled(ImVec2(p.x, p.y + 4), ImVec2(p.x + 3, p.y + 18), accent, 2.0f);
    ImGui::Dummy(ImVec2(8, 0));
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, kTextHi);
    ImGui::PushFont(g_fontTitle);
    ImGui::TextUnformatted(label);
    ImGui::PopFont();
    ImGui::PopStyleColor();
}

static void DrawThinSeparator()
{
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList *dl = ImGui::GetWindowDrawList();
    dl->AddLine(ImVec2(p.x + 4, p.y + 1), ImVec2(p.x + ImGui::GetContentRegionAvail().x - 4, p.y + 1),
                IM_COL32(120, 80, 180, 90), 1.0f);
    ImGui::Dummy(ImVec2(0, 6));
}

// Convert a wchar_t* to a UTF-8 std::string for ImGui display.
static std::string WideToUtf8(const wchar_t *w)
{
    if (!w || !*w)
        return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0)
        return {};
    std::string s(static_cast<std::size_t>(len - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], len, nullptr, nullptr);
    return s;
}

static void DrawWindowPickerModal()
{
    if (!g_showWindowPicker)
        return;

    ImGui::OpenPopup(aimlab::i18n::tr("picker_title"));
    ImGui::SetNextWindowSize(ImVec2(720, 460), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal(aimlab::i18n::tr("picker_title"), &g_showWindowPicker,
                               ImGuiWindowFlags_NoResize))
    {
        // Refresh the cached list the first frame the modal opens.
        if (g_pickerCacheDirty)
        {
            g_pickerCache = aimlab::EnumerateVisibleWindows(g_hwnd);
            g_pickerCacheDirty = false;
        }

        // Top toolbar: search box + refresh button.
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 110);
        ImGui::InputTextWithHint("##filter", aimlab::i18n::tr("picker_filter_hint"),
                                 g_pickerFilter, sizeof(g_pickerFilter));
        ImGui::PopItemWidth();
        ImGui::SameLine();
        if (ImGui::Button(aimlab::i18n::tr("picker_refresh"), ImVec2(100, 0)))
        {
            g_pickerCacheDirty = true;
        }

        ImGui::Dummy(ImVec2(0, 4));
        DrawThinSeparator();

        // Table of windows. Layout: Title | Class | PID | Size.
        ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable;
        if (ImGui::BeginTable("winlist", 4, flags, ImVec2(0, 300)))
        {
            ImGui::TableSetupColumn(aimlab::i18n::tr("picker_col_title"), ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn(aimlab::i18n::tr("picker_col_class"), ImGuiTableColumnFlags_WidthFixed, 140.0f);
            ImGui::TableSetupColumn(aimlab::i18n::tr("picker_col_pid"), ImGuiTableColumnFlags_WidthFixed, 64.0f);
            ImGui::TableSetupColumn(aimlab::i18n::tr("picker_col_size"), ImGuiTableColumnFlags_WidthFixed, 120.0f);
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableHeadersRow();

            // Lowercase the filter once for the per-row comparisons.
            std::string needle;
            if (g_pickerFilter[0])
            {
                needle = g_pickerFilter;
                for (auto &c : needle)
                    c = (char)std::tolower((unsigned char)c);
            }
            auto contains = [&](const wchar_t *w) -> bool
            {
                if (needle.empty())
                    return true;
                std::string s = WideToUtf8(w);
                for (auto &c : s)
                    c = (char)std::tolower((unsigned char)c);
                return s.find(needle) != std::string::npos;
            };

            for (size_t i = 0; i < g_pickerCache.size(); ++i)
            {
                const auto &w = g_pickerCache[i];
                if (!contains(w.title.c_str()) && !contains(w.className.c_str()))
                    continue;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                const bool wasSelected = false;
                (void)wasSelected;
                std::string titleUtf8 = WideToUtf8(w.title.c_str());
                if (w.minimized)
                    titleUtf8 += aimlab::i18n::tr("picker_minimized");
                if (ImGui::Selectable(titleUtf8.c_str(), false,
                                      ImGuiSelectableFlags_SpanAllColumns |
                                          ImGuiSelectableFlags_AllowDoubleClick))
                {
                    g_cfg->setScope(aimlab::WindowScope::SpecificWindow);
                    g_cfg->setTargetWindow(w.title.c_str(), w.className.c_str(), (int)w.pid);
                    g_showWindowPicker = false;
                    ImGui::CloseCurrentPopup();
                }
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip(aimlab::i18n::tr("picker_dblclick"));
                }
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(WideToUtf8(w.className.c_str()).c_str());
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%lu", (unsigned long)w.pid);
                ImGui::TableSetColumnIndex(3);
                int ww = w.rect.right - w.rect.left;
                int hh = w.rect.bottom - w.rect.top;
                ImGui::Text("%d x %d", ww, hh);
            }
            ImGui::EndTable();
        }

        // Footer: count + close.
        ImGui::Dummy(ImVec2(0, 4));
        ImGui::TextDisabled(aimlab::i18n::tr("picker_count"), g_pickerCache.size());

        ImGui::SetCursorPosY(ImGui::GetWindowSize().y - 36);
        ImGui::Separator();
        if (ImGui::Button(aimlab::i18n::tr("picker_close"), ImVec2(120, 0)) ||
            ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape)))
        {
            g_showWindowPicker = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

static void DrawControlPanel()
{
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(900, 800), ImGuiCond_Always);

    ImGui::Begin("aimlab", nullptr,
                 ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoTitleBar);

    // Sync the current language into the i18n module so tr() can be a
    // one-argument lookup everywhere in this frame.
    aimlab::i18n::g_currentLanguage = g_cfg->view().language;

    const bool active = g_cfg->aimEnabled();

    // ====== Top: logo + title row (also serves as the drag handle area) ======
    {
        // Logo on the left (PNG is transparent - no frame needed)
        if (g_logo.glId)
        {
            const float targetH = 116.0f;
            const float aspect = (float)g_logo.width / (float)g_logo.height;
            const float targetW = targetH * aspect;
            ImGui::Image((ImTextureID)(intptr_t)g_logo.glId,
                         ImVec2(targetW, targetH));
            ImGui::SameLine();
        }

        // Title + subtitle + Discord link
        ImGui::PushFont(g_fontTitle);
        ImGui::PushStyleColor(ImGuiCol_Text, kPurpleBright);
        ImGui::TextUnformatted(aimlab::i18n::tr("title"));
        ImGui::PopStyleColor();
        ImGui::PopFont();

        ImGui::PushStyleColor(ImGuiCol_Text, kTextMid);
        ImGui::Text("  %s", aimlab::i18n::tr("subtitle"));
        ImGui::PopStyleColor();

        // Discord link (clickable, underlined)
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.70f, 1.00f, 1.0f));
        ImGui::Text("  -  ");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.18f, 0.40f, 0.50f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.12f, 0.32f, 0.70f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.70f, 1.00f, 1.0f));
        if (ImGui::Button(aimlab::i18n::tr("discord")))
        {
            if (kDiscordUrl && kDiscordUrl[0])
                ShellExecuteA(nullptr, "open", kDiscordUrl, nullptr, nullptr, SW_SHOWNORMAL);
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(kDiscordUrl);
        ImGui::PopStyleColor(4);

        // Status dot + close button: drawn as window-relative overlay in the
        // top-right corner. They do NOT participate in the ImGui layout flow -
        // we save the cursor, draw the overlay, then restore the cursor so the
        // next widget (separator / AIM button) starts on a clean line.
        const float winW = ImGui::GetWindowSize().x;
        const ImVec2 savedCursor = ImGui::GetCursorPos();

        const float dotR = 6.0f;
        const float dotCx = winW - 50.0f;
        const float dotCy = 18.0f;
        ImU32 dotCol = active ? IM_COL32(170, 100, 255, 255)
                              : IM_COL32(90, 70, 120, 200);
        ImDrawList *dl = ImGui::GetWindowDrawList();
        if (active)
        {
            // soft glow around the dot
            dl->AddCircleFilled(ImVec2(dotCx, dotCy), dotR + 4.0f,
                                IM_COL32(170, 100, 255, 50), 24);
            dl->AddCircleFilled(ImVec2(dotCx, dotCy), dotR + 2.0f,
                                IM_COL32(170, 100, 255, 100), 24);
        }
        dl->AddCircleFilled(ImVec2(dotCx, dotCy), dotR, dotCol, 24);

        // Close button (X) in the very top-right corner
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.40f, 0.15f, 0.30f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.65f, 0.20f, 0.35f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.50f, 0.15f, 0.25f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
        ImGui::SetCursorPos(ImVec2(winW - 32.0f, 4.0f));
        if (ImGui::Button("X", ImVec2(28, 28)))
        {
            PostQuitMessage(0);
        }
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(aimlab::i18n::tr("close"));
        }
        ImGui::PopStyleColor(4);

        // Restore the cursor so the rest of the layout is unaffected.
        ImGui::SetCursorPos(savedCursor);
    }
    DrawThinSeparator();

    // ====== Big AIM button with glow ======
    {
        const ImVec2 btnSize(ImGui::GetContentRegionAvail().x, 56.0f);
        ImVec2 btnPos = ImGui::GetCursorScreenPos();

        if (active)
        {
            // Outer purple glow
            ImDrawList *dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(
                ImVec2(btnPos.x - 4, btnPos.y - 4),
                ImVec2(btnPos.x + btnSize.x + 4, btnPos.y + btnSize.y + 4),
                IM_COL32(160, 95, 255, 60), 14.0f);
            dl->AddRectFilled(
                ImVec2(btnPos.x - 2, btnPos.y - 2),
                ImVec2(btnPos.x + btnSize.x + 2, btnPos.y + btnSize.y + 2),
                IM_COL32(180, 120, 255, 90), 12.0f);
        }

        ImVec4 col = active ? kPurple : ImVec4(0.15f, 0.10f, 0.24f, 1.0f);
        ImVec4 colHov = active ? kPurpleBright : ImVec4(0.22f, 0.14f, 0.34f, 1.0f);
        ImVec4 colAct = active ? kPurpleDeep : ImVec4(0.18f, 0.11f, 0.28f, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_Button, col);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colHov);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, colAct);
        ImGui::PushStyleColor(ImGuiCol_Text, active ? ImVec4(1, 1, 1, 1) : kTextMid);
        ImGui::PushFont(g_fontTitle);

        const char *label = active ? aimlab::i18n::tr("aim_on") : aimlab::i18n::tr("aim_off");
        if (ImGui::Button(label, btnSize))
            g_cfg->setAimEnabled(!active);

        ImGui::PopFont();
        ImGui::PopStyleColor(4);

        ImGui::Dummy(ImVec2(0, 4));
        // Hotkey row: shows current combo + a "Change..." button
        const aimlab::Settings &shk = g_cfg->view();
        std::string hkStr = HotkeyToString(shk.hotkeyMod, shk.hotkeyVk);
        ImGui::PushStyleColor(ImGuiCol_Text, kTextLo);
        ImGui::TextUnformatted(aimlab::i18n::tr("hotkey_label"));
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (g_capturingHotkey)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, kPurpleBright);
            ImGui::TextUnformatted(aimlab::i18n::tr("hotkey_capture"));
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, kTextHi);
            ImGui::Text("%s", hkStr.c_str());
            ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.12f, 0.32f, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.20f, 0.55f, 0.95f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.28f, 0.15f, 0.45f, 1.00f));
            if (ImGui::Button(aimlab::i18n::tr("hotkey_change")))
            {
                g_capturingHotkey = true;
            }
            ImGui::PopStyleColor(3);
        }
    }
    ImGui::Dummy(ImVec2(0, 6));
    DrawThinSeparator();

    // ====== Settings (language + performance) ======
    DrawSectionHeader(aimlab::i18n::tr("section_settings"));
    {
        const aimlab::Settings &s = g_cfg->view();
        int langIdx = (int)s.language;
        const char *langItems = "English\0PortuguÃªs\0";
        ImGui::PushItemWidth(180);
        if (ImGui::Combo(aimlab::i18n::tr("language"), &langIdx, langItems))
        {
            g_cfg->setLanguage((aimlab::Language)langIdx);
            aimlab::i18n::g_currentLanguage = (aimlab::Language)langIdx;
        }
        ImGui::PopItemWidth();

        int sr = s.searchRegionPx;
        if (ImGui::SliderInt(aimlab::i18n::tr("search_region"), &sr, 200, 1600))
            g_cfg->setSearchRegion(sr);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(aimlab::i18n::tr("search_region_tt"));

        int fd = s.frameDelayMs;
        if (ImGui::SliderInt(aimlab::i18n::tr("frame_delay"), &fd, 0, 32))
            g_cfg->setFrameDelay(fd);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(aimlab::i18n::tr("frame_delay_tt"));
    }
    ImGui::Dummy(ImVec2(0, 4));
    DrawThinSeparator();

    // ====== Presets ======
    DrawSectionHeader(aimlab::i18n::tr("section_presets"));
    {
        static int presetIdx = 0;
        if (presetIdx >= (int)g_presets.size())
            presetIdx = 0;

        // Pick a localized label for each built-in; user presets show their
        // own name verbatim.
        auto presetLabel = [](int i) -> const char *
        {
            if (i == 0)
                return aimlab::i18n::tr("default");
            if (i == 1)
                return aimlab::i18n::tr("aggressive");
            if (i == 2)
                return aimlab::i18n::tr("conservative");
            if (i == 3)
                return aimlab::i18n::tr("sniper");
            return g_presets[i].name.c_str();
        };
        const std::string preview = presetLabel(presetIdx);

        ImGui::PushItemWidth(220);
        if (ImGui::BeginCombo("##preset", preview.c_str()))
        {
            for (int i = 0; i < (int)g_presets.size(); ++i)
            {
                bool sel = (i == presetIdx);
                const char *lbl = presetLabel(i);
                if (i == 4 && g_presets.size() > 4)
                {
                    // Visual separator between built-ins and user presets
                    ImGui::PushStyleColor(ImGuiCol_Text, kTextLo);
                    ImGui::TextUnformatted(aimlab::i18n::tr("preset_separator"));
                    ImGui::PopStyleColor();
                }
                if (ImGui::Selectable(lbl, sel))
                    presetIdx = i;
                if (sel)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        ImGui::PopItemWidth();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.18f, 0.70f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.52f, 0.28f, 0.95f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.42f, 0.20f, 0.82f, 1.00f));
        if (ImGui::Button(aimlab::i18n::tr("preset_apply")))
        {
            ApplyPreset(*g_cfg, g_presets[presetIdx]);
        }
        ImGui::PopStyleColor(3);
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.12f, 0.32f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.20f, 0.55f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.28f, 0.15f, 0.45f, 1.00f));
        if (ImGui::Button(aimlab::i18n::tr("preset_save")))
        {
            ImGui::OpenPopup(aimlab::i18n::tr("preset_modal_title"));
        }
        ImGui::PopStyleColor(3);

        // Modal for naming + saving
        if (ImGui::BeginPopupModal(aimlab::i18n::tr("preset_modal_title"), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            static char nameBuf[64] = "";
            const aimlab::Settings &s = g_cfg->view();
            ImGui::TextUnformatted(aimlab::i18n::tr("preset_name"));
            ImGui::SameLine();
            ImGui::PushItemWidth(260);
            bool enter = ImGui::InputText("##pname", nameBuf, sizeof(nameBuf),
                                          ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::PopItemWidth();
            ImGui::Separator();
            ImGui::Text(aimlab::i18n::tr("preset_current"),
                        s.targetR, s.targetG, s.targetB,
                        s.colorTolerance, s.aimMaxSpeed);
            if (ImGui::Button(aimlab::i18n::tr("save"), ImVec2(120, 0)) || enter)
            {
                if (nameBuf[0])
                {
                    Preset np;
                    np.name = nameBuf;
                    np.targetR = s.targetR;
                    np.targetG = s.targetG;
                    np.targetB = s.targetB;
                    np.colorTolerance = s.colorTolerance;
                    np.sampleRadiusPx = s.sampleRadiusPx;
                    np.minPixelNeighbors = s.minPixelNeighbors;
                    np.clusterRadiusPx = s.clusterRadiusPx;
                    np.minClusterSize = s.minClusterSize;
                    np.aimMaxSpeed = s.aimMaxSpeed;
                    np.aimPower = s.aimPower;
                    np.aimDeadzonePx = s.aimDeadzonePx;
                    np.aimSnapClose = s.aimSnapClose;
                    np.autoClickEnabled = s.autoClickEnabled;
                    np.autoClickDistancePx = s.autoClickDistancePx;
                    np.inputMethod = s.inputMethod;
                    g_presets.push_back(std::move(np));
                    SaveUserPresets();
                    presetIdx = (int)g_presets.size() - 1;
                    nameBuf[0] = 0;
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(aimlab::i18n::tr("cancel"), ImVec2(120, 0)))
            {
                nameBuf[0] = 0;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
    ImGui::Dummy(ImVec2(0, 4));
    DrawThinSeparator();

    // ====== Target window ======
    DrawSectionHeader(aimlab::i18n::tr("section_target_window"));
    {
        const aimlab::Settings &s = g_cfg->view();

        int scopeIdx = (int)s.scope;
        char scopeItems[64];
        std::snprintf(scopeItems, sizeof(scopeItems), "%s\0%s\0",
                      aimlab::i18n::tr("scope_full"),
                      aimlab::i18n::tr("scope_window"));
        ImGui::PushItemWidth(180);
        if (ImGui::Combo("##scope", &scopeIdx, scopeItems))
        {
            g_cfg->setScope(scopeIdx == 1
                                ? aimlab::WindowScope::SpecificWindow
                                : aimlab::WindowScope::FullScreen);
        }
        ImGui::PopItemWidth();
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(aimlab::i18n::tr("scope_tooltip"));
        }
        ImGui::SameLine();

        // Status pill showing the chosen window (or "-").
        if (s.scope == aimlab::WindowScope::SpecificWindow)
        {
            std::string t = WideToUtf8(s.targetWindowTitle);
            if (t.empty())
                t = aimlab::i18n::tr("no_window_selected");
            ImGui::PushStyleColor(ImGuiCol_Text, t.empty() ? kTextLo : kTextHi);
            ImGui::TextUnformatted(t.c_str());
            ImGui::PopStyleColor();

            // Live "found" status from the worker.
            bool found = g_worker && g_worker->targetWindowFound.load();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, found ? kPurpleBright : ImVec4(0.95f, 0.40f, 0.40f, 1.0f));
            ImGui::TextUnformatted(found ? aimlab::i18n::tr("active") : aimlab::i18n::tr("not_found"));
            ImGui::PopStyleColor();

            if (found)
            {
                int w = g_worker->targetWindowW.load();
                int h = g_worker->targetWindowH.load();
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, kTextLo);
                ImGui::Text(" %dx%d", w, h);
                ImGui::PopStyleColor();
            }
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, kTextLo);
            ImGui::TextUnformatted(aimlab::i18n::tr("searches_whole_monitor"));
            ImGui::PopStyleColor();
        }

        // "Select window..." button - full width.
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.18f, 0.70f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.52f, 0.28f, 0.95f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.42f, 0.20f, 0.82f, 1.00f));
        if (ImGui::Button(aimlab::i18n::tr("select_window"), ImVec2(ImGui::GetContentRegionAvail().x, 32)))
        {
            g_pickerCacheDirty = true;
            g_showWindowPicker = true;
        }
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(aimlab::i18n::tr("select_window_tooltip"));
        }
    }
    ImGui::Dummy(ImVec2(0, 6));
    DrawThinSeparator();

    // ====== Target color ======
    DrawSectionHeader(aimlab::i18n::tr("section_target_color"));
    {
        const aimlab::Settings &s = g_cfg->view();

        // Save cursor Y of the R/G/B line so we can place the swatch on the
        // same line (instead of guessing with magic offsets).
        const float rgbLineY = ImGui::GetCursorScreenPos().y;

        // Three DragInts side by side, then a swatch on the right.
        ImGui::PushItemWidth(70);
        int r = s.targetR, g = s.targetG, b = s.targetB;
        if (ImGui::DragInt("##R", &r, 1, 0, 255, "R %d"))
            g_cfg->setTargetColor(r, s.targetG, s.targetB);
        ImGui::SameLine();
        if (ImGui::DragInt("##G", &g, 1, 0, 255, "G %d"))
            g_cfg->setTargetColor(s.targetR, g, s.targetB);
        ImGui::SameLine();
        if (ImGui::DragInt("##B", &b, 1, 0, 255, "B %d"))
            g_cfg->setTargetColor(s.targetR, s.targetG, b);
        ImGui::PopItemWidth();

        // Color swatch (circle) on the right side, aligned with the R/G/B line
        ImVec2 swPos(ImGui::GetWindowPos().x + ImGui::GetWindowSize().x - 50.0f,
                     rgbLineY + 10.0f);
        ImVec4 swatch(s.targetR / 255.0f, s.targetG / 255.0f, s.targetB / 255.0f, 1.0f);
        ImDrawList *dl = ImGui::GetWindowDrawList();
        if (active)
        {
            dl->AddCircleFilled(swPos, 20, IM_COL32(160, 95, 255, 60), 32);
        }
        dl->AddCircleFilled(swPos, 16,
                            IM_COL32((int)(swatch.x * 255), (int)(swatch.y * 255), (int)(swatch.z * 255), 255),
                            32);
        dl->AddCircle(swPos, 16, IM_COL32(200, 170, 255, 180), 32, 1.5f);

        int radius = s.sampleRadiusPx;
        if (ImGui::SliderInt(aimlab::i18n::tr("sample_radius"), &radius, 2, 80))
            g_cfg->setSampleRadius(radius);

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.18f, 0.70f, 0.85f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.52f, 0.28f, 0.95f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.42f, 0.20f, 0.82f, 1.00f));
        if (ImGui::Button(aimlab::i18n::tr("sample_at_center"), ImVec2(ImGui::GetContentRegionAvail().x, 32)))
        {
            auto size = aimlab::GetPrimaryScreenSize();
            auto frame = aimlab::CapturePrimaryScreen();
            aimlab::RGB rgb = aimlab::SampleAtCenter(frame, size.width / 2, size.height / 2, radius);
            g_cfg->setTargetColor(rgb.r, rgb.g, rgb.b);
        }
        ImGui::PopStyleColor(3);
        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip(aimlab::i18n::tr("sample_tooltip"));
        }
    }
    ImGui::Dummy(ImVec2(0, 6));
    DrawThinSeparator();

    // ====== Detection ======
    DrawSectionHeader(aimlab::i18n::tr("section_detection"));
    {
        const aimlab::Settings &s = g_cfg->view();
        int tol = s.colorTolerance;
        if (ImGui::SliderInt(aimlab::i18n::tr("color_tolerance"), &tol, 0, 128))
            g_cfg->setColorTolerance(tol);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(aimlab::i18n::tr("color_tolerance_tt"));
        int blob = s.minPixelNeighbors;
        if (ImGui::SliderInt(aimlab::i18n::tr("min_neighbors"), &blob, 1, 9))
            g_cfg->setMinPixelNeighbors(blob);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(aimlab::i18n::tr("min_neighbors_tt"));

        ImGui::PushStyleColor(ImGuiCol_Text, kTextLo);
        ImGui::TextUnformatted(aimlab::i18n::tr("cluster_label"));
        ImGui::PopStyleColor();
        int cr = s.clusterRadiusPx;
        if (ImGui::SliderInt(aimlab::i18n::tr("cluster_radius"), &cr, 2, 40))
            g_cfg->setClusterRadius(cr);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(aimlab::i18n::tr("cluster_radius_tt"));
        int mc = s.minClusterSize;
        if (ImGui::SliderInt(aimlab::i18n::tr("min_cluster"), &mc, 1, 200))
            g_cfg->setMinClusterSize(mc);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(aimlab::i18n::tr("min_cluster_tt"));

        ImGui::PushStyleColor(ImGuiCol_Text, kTextLo);
        ImGui::TextUnformatted(aimlab::i18n::tr("lock_label"));
        ImGui::PopStyleColor();
        bool sticky = s.stickyTarget;
        if (ImGui::Checkbox(aimlab::i18n::tr("sticky"), &sticky))
            g_cfg->setStickyTarget(sticky);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(aimlab::i18n::tr("sticky_tt"));
        int sr = s.stickRadiusPx;
        if (ImGui::SliderInt(aimlab::i18n::tr("stick_radius"), &sr, 10, 200))
            g_cfg->setStickRadius(sr);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(aimlab::i18n::tr("stick_radius_tt"));
        int sw = s.stickSwitchRatio;
        if (ImGui::SliderInt(aimlab::i18n::tr("switch_threshold"), &sw, 0, 100))
            g_cfg->setStickSwitchRatio(sw);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(aimlab::i18n::tr("switch_threshold_tt"));
        int sm = s.stickLockMaxMisses;
        if (ImGui::SliderInt(aimlab::i18n::tr("max_miss"), &sm, 1, 15))
            g_cfg->setStickMaxMisses(sm);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(aimlab::i18n::tr("max_miss_tt"));
    }
    ImGui::Dummy(ImVec2(0, 4));
    DrawThinSeparator();

    // ====== Movement ======
    DrawSectionHeader(aimlab::i18n::tr("section_movement"));
    {
        const aimlab::Settings &s = g_cfg->view();
        int spd = s.aimMaxSpeed;
        if (ImGui::SliderInt(aimlab::i18n::tr("aim_speed"), &spd, 1, 200))
            g_cfg->setAimMaxSpeed(spd);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(aimlab::i18n::tr("aim_speed_tt"));

        int power = s.aimPower;
        if (ImGui::SliderInt(aimlab::i18n::tr("aim_power"), &power, 100, 300))
            g_cfg->setAimPower(power);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(aimlab::i18n::tr("aim_power_tt"));

        int dz = s.aimDeadzonePx;
        if (ImGui::SliderInt(aimlab::i18n::tr("deadzone"), &dz, 0, 50))
            g_cfg->setAimDeadzone(dz);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(aimlab::i18n::tr("deadzone_tt"));

        bool snap = s.aimSnapClose;
        if (ImGui::Checkbox(aimlab::i18n::tr("snap_close"), &snap))
            g_cfg->setAimSnapClose(snap);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(aimlab::i18n::tr("snap_close_tt"));

        int method = s.inputMethod;
        ImGui::PushStyleColor(ImGuiCol_Text, kTextMid);
        ImGui::TextUnformatted(aimlab::i18n::tr("input_method"));
        ImGui::PopStyleColor();
        ImGui::RadioButton(aimlab::i18n::tr("method_mouse_event"), &method, 0);
        ImGui::SameLine();
        ImGui::RadioButton(aimlab::i18n::tr("method_sendinput"), &method, 1);
        if (method != s.inputMethod)
            g_cfg->setInputMethod(method);
    }
    ImGui::Dummy(ImVec2(0, 4));
    DrawThinSeparator();

    // ====== Auto-click ======
    DrawSectionHeader(aimlab::i18n::tr("section_autoclick"));
    {
        const aimlab::Settings &s = g_cfg->view();
        bool ac = s.autoClickEnabled;
        if (ImGui::Checkbox(aimlab::i18n::tr("auto_click"), &ac))
            g_cfg->setAutoClick(ac);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip(aimlab::i18n::tr("auto_click_tt"));
        int cd = s.autoClickDistancePx;
        if (ImGui::SliderInt(aimlab::i18n::tr("auto_click_distance"), &cd, 1, 200))
            g_cfg->setAutoClickDistance(cd);
    }
    ImGui::Dummy(ImVec2(0, 4));
    DrawThinSeparator();

    // ====== Status ======
    DrawSectionHeader(aimlab::i18n::tr("section_status"));
    {
        ImGui::PushStyleColor(ImGuiCol_Text, kTextMid);
        ImGui::Text(aimlab::i18n::tr("status_fps"), g_worker->fps.load());
        ImGui::Text(aimlab::i18n::tr("status_frames"), (long long)g_worker->frames.load());
        ImGui::Text(aimlab::i18n::tr("status_clicks"), (long long)g_worker->clicks.load());
        ImGui::Text(aimlab::i18n::tr("status_errors"), (long long)g_worker->captureErrors.load());
        ImGui::PopStyleColor();

        if (g_worker->hasLastTarget.load())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, kTextHi);
            ImGui::Text(aimlab::i18n::tr("status_last_found"),
                        g_worker->lastTargetX.load(),
                        g_worker->lastTargetY.load(),
                        g_worker->lastTargetSize.load(),
                        g_worker->lastTargetDistPx.load());
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, kTextLo);
            ImGui::TextUnformatted(aimlab::i18n::tr("status_last_none"));
            ImGui::PopStyleColor();
        }

        bool cm = g_worker->lastCenterMatched.load();
        ImGui::PushStyleColor(ImGuiCol_Text, cm ? kPurpleBright : kTextLo);
        ImGui::Text(aimlab::i18n::tr("status_center"), cm ? aimlab::i18n::tr("status_yes") : aimlab::i18n::tr("status_no"));
        ImGui::PopStyleColor();

        bool locked = g_worker->hasLock.load();
        ImGui::PushStyleColor(ImGuiCol_Text, locked ? kPurpleBright : kTextLo);
        if (locked)
        {
            ImGui::Text(aimlab::i18n::tr("status_lock"),
                        g_worker->lockX.load(),
                        g_worker->lockY.load());
        }
        else
        {
            ImGui::TextUnformatted(aimlab::i18n::tr("status_lock_none"));
        }
        ImGui::PopStyleColor();
    }

    g_lastImguiSize = ImGui::GetWindowSize();

    ImGui::End();

    DrawWindowPickerModal();
}

// ---------- Window class & message loop ----------
struct Win32Ctx
{
    HINSTANCE inst;
    HWND hwnd;
    HDC hdc;
};

static Win32Ctx g_ctx{};

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return 1;

    switch (msg)
    {
    case WM_HOTKEY:
        if (wParam == kHotkeyId)
        {
            bool now = !g_cfg->aimEnabled();
            g_cfg->setAimEnabled(now);
        }
        return 0;

    case WM_KEYDOWN:
        if (g_capturingHotkey)
        {
            g_capturingHotkey = false;
            if (wParam == VK_ESCAPE)
                return 0; // cancel
            int mod = GetCurrentModifiers();
            int vk = (int)wParam;
            // A single-key combo (no modifier) is allowed.
            aimlab::UnregisterAimHotkey(hwnd, kHotkeyId);
            g_cfg->setHotkey(mod, vk);
            aimlab::RegisterAimHotkey(hwnd, mod, vk, kHotkeyId);
            return 0;
        }
        if (wParam == VK_ESCAPE && (GetKeyState(VK_CONTROL) & 0x8000))
        {
            PostQuitMessage(0);
            return 0;
        }
        break;

    case WM_NCHITTEST:
    {
        // Make the top ~30px of the client area act as a drag handle
        // (so the borderless window can be moved).
        LRESULT hit = DefWindowProc(hwnd, msg, wParam, lParam);
        if (hit == HTCLIENT)
        {
            POINT pt{LOWORD(lParam), HIWORD(lParam)};
            ScreenToClient(hwnd, &pt);
            if (pt.y >= 0 && pt.y < 30)
                hit = HTCAPTION;
        }
        return hit;
    }

    case WM_SIZE:
        if (g_ctx.hdc && g_glrc)
        {
            wglMakeCurrent(g_ctx.hdc, g_glrc);
            int w = LOWORD(lParam), h = HIWORD(lParam);
            glViewport(0, 0, w, h);
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
    g_ctx.inst = hInst;

    // 1) Register window class.
    WNDCLASSEX wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = "aimlab_main";
    if (!RegisterClassEx(&wc))
    {
        MessageBoxA(nullptr, "RegisterClassEx failed", "aimlab", MB_ICONERROR);
        return 1;
    }

    // 2) Create the window: topmost, borderless, draggable. The size is
    //    adjusted every frame to fit the ImGui content (auto-resize);
    //    on the first frame we center it on the primary monitor.
    const auto screen = aimlab::GetPrimaryScreenSize();
    const int initW = 900, initH = 800;
    const int initX = std::max(0, (screen.width - initW) / 2);
    const int initY = std::max(0, (screen.height - initH) / 2);

    g_ctx.hwnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        wc.lpszClassName, "macro lab",
        WS_POPUP | WS_VISIBLE,
        initX, initY, initW, initH,
        nullptr, nullptr, hInst, nullptr);
    if (!g_ctx.hwnd)
    {
        MessageBoxA(nullptr, "CreateWindowEx failed", "aimlab", MB_ICONERROR);
        return 1;
    }
    g_hwnd = g_ctx.hwnd;
    g_ctx.hdc = GetDC(g_ctx.hwnd);

    // 3) OpenGL.
    if (!InitOpenGL(g_ctx.hwnd))
    {
        MessageBoxA(nullptr, "OpenGL init failed", "aimlab", MB_ICONERROR);
        return 1;
    }

    // 3.5) Logo (must happen after OpenGL is ready so we can upload a texture).
    if (!LoadLogo(L"src\\logo\\gatoLua.png", g_logo))
    {
        // Try the alternate slash.
        LoadLogo(L"src/logo/gatoLua.png", g_logo);
    }

    // 3.6) Create HICONS for the OS window (used by the taskbar / alt-tab).
    g_hIconSmall = CreateHIconFromPng(L"src\\logo\\gatoLua.png", 16);
    g_hIconBig = CreateHIconFromPng(L"src\\logo\\gatoLua.png", 32);
    if (!g_hIconSmall)
        g_hIconSmall = g_hIconBig;
    if (g_hIconBig)
        SendMessage(g_ctx.hwnd, WM_SETICON, ICON_BIG, (LPARAM)g_hIconBig);
    if (g_hIconSmall)
        SendMessage(g_ctx.hwnd, WM_SETICON, ICON_SMALL, (LPARAM)g_hIconSmall);

    // 4) Dear ImGui.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Try to load Segoe UI (Windows 10/11) for a modern look. Fall back
    // to the default ImGui font if it isn't available.
    g_fontNormal = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 16.0f);
    if (!g_fontNormal)
        g_fontNormal = io.Fonts->AddFontDefault();
    g_fontTitle = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 24.0f);
    if (!g_fontTitle)
        g_fontTitle = g_fontNormal;
    io.Fonts->Build();

    ApplyDarkTheme();

    ImGui_ImplWin32_Init(g_ctx.hwnd);
    ImGui_ImplOpenGL3_Init("#version 130");

    // 5) Settings + worker (must exist before hotkey registration so we
    //    can read the current combo).
    aimlab::ConfigStore cfg;
    g_cfg = &cfg;
    aimlab::AimWorker worker(cfg);
    g_worker = &worker;
    worker.start();

    // 5.5) Presets: built-ins first, then user-saved from disk.
    InitBuiltinPresets();
    LoadUserPresets();

    // 6) Hotkey.
    if (!aimlab::RegisterAimHotkey(g_ctx.hwnd, cfg.view().hotkeyMod,
                                   cfg.view().hotkeyVk, kHotkeyId))
    {
        MessageBoxA(nullptr,
                    "Could not register the configured hotkey.\n"
                    "Another program may be using that combo.",
                    "aimlab", MB_ICONWARNING);
    }

    // 7) Message loop.
    MSG msg{};
    bool running = true;
    bool firstFrame = true;
    while (running)
    {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                running = false;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        if (!running)
            break;

        wglMakeCurrent(g_ctx.hdc, g_glrc);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        DrawControlPanel();
        ImGui::Render();

        // Resize the OS window to fit the ImGui content (auto-resize).
        // We captured the size inside DrawControlPanel() (during Begin/End)
        // because GetWindowSize() returns NULL deref after End().
        int w = static_cast<int>(g_lastImguiSize.x);
        int h = static_cast<int>(g_lastImguiSize.y);
        if (w < 100)
            w = 100;
        if (h < 100)
            h = 100;

        RECT rc;
        GetWindowRect(g_ctx.hwnd, &rc);
        int curW = rc.right - rc.left;
        int curH = rc.bottom - rc.top;
        if (w != curW || h != curH || firstFrame)
        {
            int x = rc.left, y = rc.top;
            if (firstFrame)
            {
                const auto s = aimlab::GetPrimaryScreenSize();
                x = std::max(0, (s.width - w) / 2);
                y = std::max(0, (s.height - h) / 2);
            }
            SetWindowPos(g_ctx.hwnd, nullptr, x, y, w, h,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            glViewport(0, 0, w, h);
            firstFrame = false;
        }

        glClearColor(0.06f, 0.07f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SwapBuffers(g_ctx.hdc);

        Sleep(8);
    }

    // 8) Cleanup.
    worker.stop();
    aimlab::UnregisterAimHotkey(g_ctx.hwnd, kHotkeyId);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    FreeLogo(g_logo);
    if (g_hIconBig)
    {
        DestroyIcon(g_hIconBig);
        g_hIconBig = nullptr;
    }
    if (g_hIconSmall)
    {
        DestroyIcon(g_hIconSmall);
        g_hIconSmall = nullptr;
    }
    ShutdownOpenGL();
    if (g_ctx.hdc)
        ReleaseDC(g_ctx.hwnd, g_ctx.hdc);
    DestroyWindow(g_ctx.hwnd);
    UnregisterClass(wc.lpszClassName, g_ctx.inst);
    return 0;
}
