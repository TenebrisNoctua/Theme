#include "Theme.h"

#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <vssym32.h>

#include <cmath>
#include <string>
#include <unordered_map>

// Version 6 of the common controls draws them in the system's visual styles. The linker puts this dependency in
// the manifest it embeds in the library, which startPlatform activates for the controls the gui library makes.
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace Luwow::Theme {

namespace {

constexpr DWORD DarkTitleBarAttribute = 20;
constexpr DWORD OldDarkTitleBarAttribute = 19;
constexpr DWORD CornersAttribute = 33;

// The colors a window and its controls are drawn in
struct Palette {
    COLORREF background;
    COLORREF text;
    // A text field's and a dropdown list's
    COLORREF field;
    COLORREF disabledText;
};

const Palette DarkPalette = { RGB(32, 32, 32), RGB(255, 255, 255), RGB(43, 43, 43), RGB(128, 128, 128) };

// The system's colors, which high contrast changes too
Palette lightPalette() {
    return { GetSysColor(COLOR_WINDOW), GetSysColor(COLOR_WINDOWTEXT), GetSysColor(COLOR_WINDOW), GetSysColor(COLOR_GRAYTEXT) };
}

// A window themed, with what it's drawn with
struct ThemedWindow {
    // Its own options, over the defaults
    Options options;
    bool dark = false;
    Palette palette = {};
    HFONT font = nullptr;
    HBRUSH background = nullptr;
    HBRUSH field = nullptr;

    void release() {
        if (font) DeleteObject(font);
        if (background) DeleteObject(background);
        if (field) DeleteObject(field);
        font = nullptr;
        background = nullptr;
        field = nullptr;
    }
};

// Only used on the main thread
std::unordered_map<HWND, ThemedWindow> windows;
Options defaults;
ULONG_PTR activationCookie = 0;

enum class Kind { Button, Checkbox, TextBox, TextArea, Dropdown, Label, Other };

Kind kindOf(HWND control) {
    wchar_t name[32] = {};
    GetClassNameW(control, name, 32);
    LONG style = GetWindowLongW(control, GWL_STYLE);
    if (_wcsicmp(name, WC_BUTTONW) == 0) {
        LONG type = style & BS_TYPEMASK;
        bool checkbox = type == BS_CHECKBOX || type == BS_AUTOCHECKBOX || type == BS_3STATE || type == BS_AUTO3STATE;
        return checkbox ? Kind::Checkbox : Kind::Button;
    }
    if (_wcsicmp(name, WC_EDITW) == 0) return style & ES_MULTILINE ? Kind::TextArea : Kind::TextBox;
    if (_wcsicmp(name, WC_COMBOBOXW) == 0) return Kind::Dropdown;
    if (_wcsicmp(name, WC_STATICW) == 0) return Kind::Label;
    return Kind::Other;
}

DWORD windowsBuild() {
    using RtlGetVersion = LONG (WINAPI*)(OSVERSIONINFOW*);
    static DWORD build = [] {
        auto getVersion = reinterpret_cast<RtlGetVersion>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion"));
        OSVERSIONINFOW version = { sizeof(version) };
        return getVersion && getVersion(&version) == 0 ? version.dwBuildNumber : 0;
    }();
    return build;
}

bool highContrast() {
    HIGHCONTRASTW contrast = { sizeof(contrast) };
    return SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) && (contrast.dwFlags & HCF_HIGHCONTRASTON);
}

// Whether the user picked dark for apps in the system's settings
bool systemDark() {
    DWORD light = 1;
    DWORD size = sizeof(light);
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size);
    return light == 0;
}

// High contrast keeps its own colors
bool useDark(ColorScheme scheme) {
    if (highContrast()) return false;
    return scheme == ColorScheme::Dark || (scheme == ColorScheme::System && systemDark());
}

// The popup menus, such as a text field's, follow the app's mode.
void updateMenus() {
    using SetPreferredAppMode = int (WINAPI*)(int);
    using FlushMenuThemes = void (WINAPI*)();
    static HMODULE uxtheme = windowsBuild() >= 18362 ? GetModuleHandleW(L"uxtheme.dll") : nullptr;
    static auto setMode = uxtheme ? reinterpret_cast<SetPreferredAppMode>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(135))) : nullptr;
    static auto flush = uxtheme ? reinterpret_cast<FlushMenuThemes>(GetProcAddress(uxtheme, MAKEINTRESOURCEA(136))) : nullptr;
    if (!setMode) return;
    bool dark = false;
    for (const auto& [window, theme] : windows) dark = dark || theme.dark;
    // ForceDark, or Default
    setMode(dark ? 2 : 0);
    if (flush) flush();
}

std::wstring wide(const std::string& text) {
    int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0);
    std::wstring result(length, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), result.data(), length);
    return result;
}

// The system's message font, Segoe UI, with the options' name and size
HFONT makeFont(HWND window, const Options& options) {
    NONCLIENTMETRICSW metrics = {};
    metrics.cbSize = sizeof(metrics);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
    LOGFONTW font = metrics.lfMessageFont;
    if (options.fontName) wcsncpy_s(font.lfFaceName, LF_FACESIZE, wide(*options.fontName).c_str(), _TRUNCATE);
    if (options.fontSize) font.lfHeight = -(LONG)std::lround(*options.fontSize * GetDpiForWindow(window) / 72.0);
    return CreateFontIndirectW(&font);
}

void themeControl(HWND control, const ThemedWindow& theme) {
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(theme.font), TRUE);
    // The dark visual styles Explorer and the file dialog use, nullptr for the system's
    const wchar_t* style = nullptr;
    switch (kindOf(control)) {
        case Kind::Button:
        case Kind::Checkbox:
        case Kind::TextArea:
            style = theme.dark ? L"DarkMode_Explorer" : nullptr;
            break;
        case Kind::TextBox:
            style = theme.dark ? L"DarkMode_CFD" : nullptr;
            break;
        case Kind::Dropdown:
        {
            style = theme.dark ? L"DarkMode_CFD" : nullptr;
            COMBOBOXINFO info = { sizeof(info) };
            if (GetComboBoxInfo(control, &info) && info.hwndList) {
                SetWindowTheme(info.hwndList, theme.dark ? L"DarkMode_Explorer" : nullptr, nullptr);
            }
            break;
        }
        case Kind::Label:
        case Kind::Other:
            return;
    }
    SetWindowTheme(control, style, nullptr);
}

// Draws a check box in the dark, whose caption the visual styles would draw black
LRESULT drawCheckbox(NMCUSTOMDRAW* draw, const ThemedWindow& theme) {
    if (draw->dwDrawStage != CDDS_PREPAINT) return CDRF_DODEFAULT;
    HWND box = draw->hdr.hwndFrom;
    HTHEME visual = OpenThemeData(box, L"Button");
    if (!visual) return CDRF_DODEFAULT;

    UINT item = draw->uItemState;
    bool disabled = item & CDIS_DISABLED;
    int state = disabled ? CBS_UNCHECKEDDISABLED
        : (item & CDIS_SELECTED) ? CBS_UNCHECKEDPRESSED
        : (item & CDIS_HOT) ? CBS_UNCHECKEDHOT
        : CBS_UNCHECKEDNORMAL;
    // The checked and mixed states follow the unchecked ones in the same order
    LRESULT check = SendMessageW(box, BM_GETCHECK, 0, 0);
    if (check == BST_CHECKED) state += CBS_CHECKEDNORMAL - CBS_UNCHECKEDNORMAL;
    else if (check == BST_INDETERMINATE) state += CBS_MIXEDNORMAL - CBS_UNCHECKEDNORMAL;

    HDC dc = draw->hdc;
    RECT bounds = draw->rc;
    FillRect(dc, &bounds, theme.background);
    SIZE glyph = { 13, 13 };
    GetThemePartSize(visual, dc, BP_CHECKBOX, state, nullptr, TS_DRAW, &glyph);
    RECT mark = { bounds.left, bounds.top + (bounds.bottom - bounds.top - glyph.cy) / 2, 0, 0 };
    mark.right = mark.left + glyph.cx;
    mark.bottom = mark.top + glyph.cy;
    DrawThemeBackground(visual, dc, BP_CHECKBOX, state, &mark, nullptr);
    CloseThemeData(visual);

    int length = GetWindowTextLengthW(box);
    std::wstring caption(length + 1, L'\0');
    length = GetWindowTextW(box, caption.data(), length + 1);
    // The gap the visual styles leave between the box and its caption
    RECT text = { mark.right + glyph.cx / 3, bounds.top, bounds.right, bounds.bottom };
    LRESULT cues = SendMessageW(box, WM_QUERYUISTATE, 0, 0);
    UINT format = DT_SINGLELINE | DT_VCENTER | DT_LEFT | ((cues & UISF_HIDEACCEL) ? DT_HIDEPREFIX : 0);

    HFONT font = reinterpret_cast<HFONT>(SendMessageW(box, WM_GETFONT, 0, 0));
    HGDIOBJ previous = font ? SelectObject(dc, font) : nullptr;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, disabled ? theme.palette.disabledText : theme.palette.text);
    DrawTextW(dc, caption.c_str(), length, &text, format);
    if ((item & CDIS_FOCUS) && !(cues & UISF_HIDEFOCUS) && length > 0) {
        RECT focus = text;
        DrawTextW(dc, caption.c_str(), length, &focus, format | DT_CALCRECT);
        int height = focus.bottom - focus.top;
        focus.top = text.top + (text.bottom - text.top - height) / 2;
        focus.bottom = focus.top + height;
        InflateRect(&focus, 1, 0);
        DrawFocusRect(dc, &focus);
    }
    if (previous) SelectObject(dc, previous);
    return CDRF_SKIPDEFAULT;
}

// Themes the window and its controls with its options over the defaults
void themeWindow(HWND window, ThemedWindow& theme) {
    Options options = defaults;
    options.merge(theme.options);
    theme.release();
    theme.dark = useDark(options.colorScheme.value_or(ColorScheme::System));
    theme.palette = theme.dark ? DarkPalette : lightPalette();
    theme.background = CreateSolidBrush(theme.palette.background);
    theme.field = CreateSolidBrush(theme.palette.field);
    theme.font = makeFont(window, options);

    // The sunken edge around the client area is the classic look
    LONG_PTR extended = GetWindowLongPtrW(window, GWL_EXSTYLE);
    if (extended & WS_EX_CLIENTEDGE) {
        SetWindowLongPtrW(window, GWL_EXSTYLE, extended & ~WS_EX_CLIENTEDGE);
        SetWindowPos(window, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }

    BOOL dark = theme.dark;
    if (FAILED(DwmSetWindowAttribute(window, DarkTitleBarAttribute, &dark, sizeof(dark)))) {
        DwmSetWindowAttribute(window, OldDarkTitleBarAttribute, &dark, sizeof(dark));
    }
    // DWMWCP_DEFAULT, DONOTROUND, ROUND and ROUNDSMALL
    Corners corners = options.corners.value_or(Corners::Default);
    DWORD corner = corners == Corners::Square ? 1 : corners == Corners::Round ? 2 : corners == Corners::Small ? 3 : 0;
    DwmSetWindowAttribute(window, CornersAttribute, &corner, sizeof(corner));

    for (HWND control = GetWindow(window, GW_CHILD); control; control = GetWindow(control, GW_HWNDNEXT)) {
        themeControl(control, theme);
    }
    updateMenus();
    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
}

LRESULT CALLBACK ThemedProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR id, DWORD_PTR data) {
    (void)data;
    auto found = windows.find(window);
    if (found == windows.end()) return DefSubclassProc(window, message, wParam, lParam);
    ThemedWindow& theme = found->second;

    switch (message) {
        // A control the gui library added
        case WM_PARENTNOTIFY:
            if (LOWORD(wParam) == WM_CREATE) themeControl(reinterpret_cast<HWND>(lParam), theme);
            break;
        // Labels, check boxes and read-only text fields
        case WM_CTLCOLORSTATIC:
        {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, IsWindowEnabled(reinterpret_cast<HWND>(lParam)) ? theme.palette.text : theme.palette.disabledText);
            SetBkColor(dc, theme.palette.background);
            return reinterpret_cast<LRESULT>(theme.background);
        }
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
        {
            if (!theme.dark) break;
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, theme.palette.text);
            SetBkColor(dc, theme.palette.field);
            return reinterpret_cast<LRESULT>(theme.field);
        }
        // Around a button's rounded corners
        case WM_CTLCOLORBTN:
            return reinterpret_cast<LRESULT>(theme.background);
        case WM_ERASEBKGND:
        {
            RECT client;
            GetClientRect(window, &client);
            FillRect(reinterpret_cast<HDC>(wParam), &client, theme.background);
            return 1;
        }
        case WM_NOTIFY:
        {
            auto* header = reinterpret_cast<NMHDR*>(lParam);
            if (theme.dark && header->code == NM_CUSTOMDRAW && kindOf(header->hwndFrom) == Kind::Checkbox) {
                return drawCheckbox(reinterpret_cast<NMCUSTOMDRAW*>(lParam), theme);
            }
            break;
        }
        // The system's scheme, contrast or font changed
        case WM_SETTINGCHANGE:
        {
            bool scheme = lParam && lstrcmpW(reinterpret_cast<LPCWSTR>(lParam), L"ImmersiveColorSet") == 0;
            if (scheme || wParam == SPI_SETNONCLIENTMETRICS || wParam == SPI_SETHIGHCONTRAST) themeWindow(window, theme);
            break;
        }
        case WM_SYSCOLORCHANGE:
            themeWindow(window, theme);
            break;
        case WM_NCDESTROY:
            theme.release();
            windows.erase(found);
            RemoveWindowSubclass(window, ThemedProc, id);
            updateMenus();
            break;
    }
    return DefSubclassProc(window, message, wParam, lParam);
}

} // namespace

void startPlatform() {
    // Pinned, so the windows' subclass stays loaded for as long as they're around
    HMODULE module = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&startPlatform), &module);
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(module, path, MAX_PATH);

    // The library's manifest, or the program's when the library is bound into it. Left active on the main
    // thread, where the gui library makes its controls, so they're version 6's.
    for (LPCWSTR resource : { MAKEINTRESOURCEW(2), MAKEINTRESOURCEW(1) }) {
        ACTCTXW context = {};
        context.cbSize = sizeof(context);
        context.dwFlags = ACTCTX_FLAG_HMODULE_VALID | ACTCTX_FLAG_RESOURCE_NAME_VALID;
        context.hModule = module;
        context.lpSource = path;
        context.lpResourceName = resource;
        HANDLE activation = CreateActCtxW(&context);
        if (activation == INVALID_HANDLE_VALUE) continue;
        if (ActivateActCtx(activation, &activationCookie)) break;
        ReleaseActCtx(activation);
    }

    INITCOMMONCONTROLSEX controls = { sizeof(controls), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&controls);
}

void applyTheme(void* native, const Options& options) {
    HWND window = static_cast<HWND>(native);
    if (!IsWindow(window)) return;
    auto [found, added] = windows.try_emplace(window);
    if (added && !SetWindowSubclass(window, ThemedProc, 1, 0)) {
        windows.erase(found);
        return;
    }
    found->second.options.merge(options);
    themeWindow(window, found->second);
}

void configureTheme(const Options& options) {
    defaults.merge(options);
    for (auto& [window, theme] : windows) themeWindow(window, theme);
}

} // namespace Luwow::Theme
