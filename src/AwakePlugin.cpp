#include "AwakePlugin.h"
#include "AwakeOptionsDialog.h"

#include <windows.h>
#include <objidl.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <gdiplus.h>
#include <utility>

#pragma comment(lib, "gdiplus.lib")

namespace
{
using namespace Gdiplus;
constexpr UINT IDM_OFF = 2001;
constexpr UINT IDM_INDEFINITE = 2002;
constexpr UINT IDM_30_MIN = 2003;
constexpr UINT IDM_1_HOUR = 2004;
constexpr UINT IDM_2_HOURS = 2005;
constexpr UINT IDM_4_HOURS = 2006;
constexpr UINT IDM_KEEP_DISPLAY = 2007;
constexpr UINT IDM_OPTIONS = 2008;
constexpr wchar_t kModuleAnchor[] = L"TrafficMonitorAwakeModuleAnchor";
// The host sends drawing context immediately before each item. Keep it
// local to the drawing thread rather than sharing main/taskbar state.
thread_local bool drawingTaskbar = false;

bool IsPresetChecked(const AwakeManager::Snapshot& snap, unsigned int minutes)
{
    return snap.mode == AwakeManager::Mode::Timed && snap.durationMinutes == minutes;
}

COLORREF StatusColor(const AwakeManager::Snapshot& snap, bool darkMode)
{
    if (!snap.requestApplied && snap.lastError != ERROR_SUCCESS)
        return RGB(220, 62, 72);
    if (snap.mode == AwakeManager::Mode::Off)
        return darkMode ? RGB(51, 65, 85) : RGB(226, 232, 240);
    if (!snap.requestApplied)
        return RGB(220, 62, 72);
    if (snap.mode == AwakeManager::Mode::Indefinite)
        return RGB(5, 150, 105);
    return RGB(217, 119, 6);
}

void AddRoundedBox(GraphicsPath& path, const RectF& rect, REAL radius)
{
    const REAL maxRadius = std::min(rect.Width, rect.Height) / 2.0f;
    radius = std::clamp(radius, 0.0f, maxRadius);
    if (radius <= 0.0f)
    {
        path.AddRectangle(rect);
        return;
    }
    const REAL diameter = radius * 2.0f;
    path.AddArc(rect.X, rect.Y, diameter, diameter, 180.0f, 90.0f);
    path.AddArc(rect.GetRight() - diameter, rect.Y, diameter, diameter, 270.0f, 90.0f);
    path.AddArc(rect.GetRight() - diameter, rect.GetBottom() - diameter,
                diameter, diameter, 0.0f, 90.0f);
    path.AddArc(rect.X, rect.GetBottom() - diameter, diameter, diameter, 90.0f, 90.0f);
    path.CloseFigure();
}


void DrawStatusIcon(HDC hdc, int x, int y, int w, int h,
                    const AwakeManager::Snapshot& snap, bool darkMode, int dpi)
{
    if (!hdc || w <= 4 || h <= 4)
        return;

    static ULONG_PTR gdiplusToken = [] {
        GdiplusStartupInput input{};
        ULONG_PTR token = 0;
        GdiplusStartup(&token, &input, nullptr);
        return token;
    }();
    if (!gdiplusToken)
        return;

    // The host may reserve a wide cell for a custom item. That width is not
    // DPI; deriving scale from it stretches a capsule into a long bar.
    const float scale = std::max(1.0f, dpi / 96.0f);
    const bool doubleLine = h > 22.0f * scale;
    const REAL capsuleWidth = std::min(34.0f * scale,
                                       static_cast<REAL>(w) - 2.0f * scale);
    // Preserve a horizontal capsule even in a narrow two-row host cell.
    const REAL capsuleHeight = std::min({static_cast<REAL>(h) - 2.0f * scale,
                                         (doubleLine ? 24.0f : 18.0f) * scale,
                                         capsuleWidth / 1.4f});
    if (capsuleHeight < 6.0f || capsuleWidth < 8.0f)
        return;

    const REAL cx = static_cast<REAL>(x) + w * 0.5f;
    const REAL cy = static_cast<REAL>(y) + h * 0.5f;
    const RectF capsule(cx - capsuleWidth / 2.0f,
                        cy - capsuleHeight / 2.0f,
                        capsuleWidth, capsuleHeight);
    const REAL size = capsuleHeight * 0.72f;
    const bool hasDisplayBadge = snap.keepDisplayOn && snap.requestApplied && snap.mode != AwakeManager::Mode::Off;
    const bool hasError = !snap.requestApplied &&
        (snap.mode != AwakeManager::Mode::Off || snap.lastError != ERROR_SUCCESS);
    const COLORREF rgb = StatusColor(snap, darkMode);
    const Color status(255, GetRValue(rgb), GetGValue(rgb), GetBValue(rgb));
    // Render only the badge at 4x resolution; the buffer is independent of the
    // host cell width. Downsampling keeps small curves and stems consistent.
    constexpr int kRenderScale = 4;
    constexpr REAL kRenderMargin = 2.0f;
    const int imageWidth = static_cast<int>(std::ceil(capsuleWidth + kRenderMargin * 2.0f));
    const int imageHeight = static_cast<int>(std::ceil(capsuleHeight + kRenderMargin * 2.0f));
    const REAL imageX = cx - imageWidth * 0.5f;
    const REAL imageY = cy - imageHeight * 0.5f;
    Bitmap badge(imageWidth * kRenderScale, imageHeight * kRenderScale, PixelFormat32bppPARGB);
    if (badge.GetLastStatus() != Ok)
        return;
    Graphics graphics(&badge);
    graphics.Clear(Color(0, 0, 0, 0));
    graphics.ScaleTransform(static_cast<REAL>(kRenderScale), static_cast<REAL>(kRenderScale));
    graphics.TranslateTransform(-imageX, -imageY);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(PixelOffsetModeHalf);
    graphics.SetCompositingMode(CompositingModeSourceOver);

    // Opaque, borderless fill avoids a dark rim and host-dependent alpha blend.
    GraphicsPath capsulePath;
    AddRoundedBox(capsulePath, capsule, capsule.Height / 2.0f);
    SolidBrush background(status);
    graphics.FillPath(&background, &capsulePath);
    const bool inactive = snap.mode == AwakeManager::Mode::Off && !hasError;
    const Color glyphColor = inactive
        ? (darkMode ? Color(255, 203, 213, 225) : Color(255, 71, 85, 105))
        : Color(255, 255, 255, 255);
    const REAL stroke = std::max(1.0f, size * 0.105f + 0.15f * scale);
    Pen glyph(glyphColor, stroke);
    glyph.SetStartCap(LineCapRound);
    glyph.SetEndCap(LineCapRound);
    glyph.SetLineJoin(LineJoinRound);
    const REAL radius = (size - stroke) / 2.0f;
    const RectF ring(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    if (hasDisplayBadge)
    {
        // A single open screen silhouette remains legible at 100% DPI.
        // Capsule color still distinguishes indefinite and timed wake modes.
        const REAL screenHeight = size * 0.62f;
        const REAL standHeight = size * 0.20f;
        const REAL screenTop = cy - (screenHeight + standHeight) / 2.0f;
        const RectF screen(cx - size * 0.55f, screenTop,
                           size * 1.10f, screenHeight);
        GraphicsPath screenPath;
        AddRoundedBox(screenPath, screen, size * 0.10f);
        graphics.DrawPath(&glyph, &screenPath);
        const REAL baseY = screen.GetBottom() + standHeight;
        graphics.DrawLine(&glyph, cx, screen.GetBottom(), cx, baseY);
        graphics.DrawLine(&glyph, cx - size * 0.20f, baseY,
                          cx + size * 0.20f, baseY);
    }
    else if (hasError)
    {
        graphics.DrawLine(&glyph, cx, cy - size * 0.30f,
                          cx, cy + size * 0.08f);
        SolidBrush dot(glyphColor);
        const REAL d = stroke / 2.0f;
        graphics.FillEllipse(&dot, cx - d, cy + size * 0.30f - d,
                             d * 2.0f, d * 2.0f);
    }
    else if (snap.mode == AwakeManager::Mode::Timed || snap.mode == AwakeManager::Mode::Until)
    {
        graphics.DrawEllipse(&glyph, ring);
        graphics.DrawLine(&glyph, cx, cy, cx, cy - radius * 0.54f);
        graphics.DrawLine(&glyph, cx, cy, cx + radius * 0.46f, cy + radius * 0.26f);
    }
    else
    {
        graphics.DrawLine(&glyph, cx, cy - radius, cx, cy - radius * 0.10f);
        graphics.DrawArc(&glyph, ring, -48.0f, 276.0f);
    }

    Graphics destination(hdc);
    destination.SetClip(Rect(x, y, w, h), CombineModeIntersect);
    destination.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    destination.SetPixelOffsetMode(PixelOffsetModeHalf);
    destination.DrawImage(&badge, RectF(imageX, imageY,
                                      static_cast<REAL>(imageWidth), static_cast<REAL>(imageHeight)),
                          0.0f, 0.0f, static_cast<REAL>(badge.GetWidth()),
                          static_cast<REAL>(badge.GetHeight()), UnitPixel);
}
}

const wchar_t* AwakeItem::GetItemName() const
{
    return L"保持唤醒";
}

const wchar_t* AwakeItem::GetItemId() const
{
    return L"TrafficMonitorAwakeState";
}

const wchar_t* AwakeItem::GetItemLableText() const
{
    return L"防休眠";
}

const wchar_t* AwakeItem::GetItemValueText() const
{
    return valueText_.c_str();
}

const wchar_t* AwakeItem::GetItemValueSampleText() const
{
    return L"99h59m";
}

bool AwakeItem::IsCustomDraw() const
{
    return true;
}

int AwakeItem::GetItemWidth() const
{
    // The host scales this value for the current DPI.
    return 40;
}

void AwakeItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode)
{
    DrawStatusIcon(static_cast<HDC>(hDC), x, y, w, h, owner_.Manager().GetSnapshot(),
                   dark_mode, owner_.GetDrawingDpi(static_cast<HDC>(hDC)));
}

int AwakeItem::IsDoubleLineExclusive() const
{
    // Let the host choose one or two rows; DrawItem adapts to the supplied height.
    return 0;
}

int AwakeItem::OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag)
{
    return owner_.HandleItemMouse(type, x, y, static_cast<HWND>(hWnd), flag);
}

void AwakeItem::SetValueText(std::wstring value)
{
    valueText_ = std::move(value);
}

AwakePlugin::AwakePlugin()
    : item_(*this)
{
}

AwakePlugin& AwakePlugin::Instance()
{
    static AwakePlugin instance;
    return instance;
}

IPluginItem* AwakePlugin::GetItem(int index)
{
    return index == 0 ? &item_ : nullptr;
}

void AwakePlugin::DataRequired()
{
    manager_.Refresh();
    item_.SetValueText(manager_.GetShortStatus());
    tooltip_ = manager_.GetTooltipText();
}

ITMPlugin::OptionReturn AwakePlugin::ShowOptionsDialog(void* hParent)
{
    const bool changed = AwakeOptionsDialog::Show(static_cast<HWND>(hParent), manager_);
    DataRequired();
    return changed ? OR_OPTION_CHANGED : OR_OPTION_UNCHANGED;
}

const wchar_t* AwakePlugin::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:
        return L"TrafficMonitor Awake";
    case TMI_DESCRIPTION:
        return L"在 TrafficMonitor 中阻止 Windows 自动休眠，支持无限、定时、指定结束时间及保持屏幕开启。";
    case TMI_AUTHOR:
        return L"";
    case TMI_COPYRIGHT:
        return L"";
    case TMI_VERSION:
        return L"1.0.2";
    case TMI_URL:
        return L"";
    default:
        return L"";
    }
}

const wchar_t* AwakePlugin::GetTooltipInfo()
{
    if (tooltip_.empty())
        tooltip_ = manager_.GetTooltipText();
    return tooltip_.c_str();
}

void AwakePlugin::OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data)
{
    if (index == EI_DRAW_TASKBAR_WND)
        drawingTaskbar = data && data[0] == L'1';
    if (index == EI_CONFIG_DIR && data && *data)
    {
        configDirFromExtendedInfo_ = data;
        manager_.SetConfigPath(ResolveConfigPath());
    }
}

int AwakePlugin::GetCommandCount()
{
    return 3;
}

const wchar_t* AwakePlugin::GetCommandName(int command_index)
{
    switch (command_index)
    {
    case 0:
        return L"切换保持唤醒";
    case 1:
        return L"保持屏幕开启";
    case 2:
        return L"Awake 设置...";
    default:
        return L"";
    }
}

void AwakePlugin::OnPluginCommand(int command_index, void* hWnd, void*)
{
    const auto snap = manager_.GetSnapshot();
    switch (command_index)
    {
    case 0:
        NotifyIfNeeded(manager_.Toggle());
        break;
    case 1:
        NotifyIfNeeded(manager_.SetKeepDisplayOn(!snap.keepDisplayOn));
        break;
    case 2:
        ShowOptionsDialog(hWnd);
        break;
    default:
        break;
    }
    DataRequired();
}

int AwakePlugin::IsCommandChecked(int command_index)
{
    const auto snap = manager_.GetSnapshot();
    if (command_index == 0)
        return snap.mode != AwakeManager::Mode::Off;
    if (command_index == 1)
        return snap.keepDisplayOn;
    return 0;
}

void AwakePlugin::OnInitialize(ITrafficMonitor* pApp)
{
    app_ = pApp;
    manager_.Initialize(ResolveConfigPath());
    DataRequired();
}

int AwakePlugin::HandleItemMouse(IPluginItem::MouseEventType type, int x, int y, HWND hwnd, int)
{
    switch (type)
    {
    case IPluginItem::MT_LCLICKED:
        NotifyIfNeeded(manager_.Toggle());
        DataRequired();
        return 1;

    case IPluginItem::MT_RCLICKED:
        ShowQuickMenu(hwnd, x, y);
        DataRequired();
        return 1;

    default:
        return 0;
    }
}

int AwakePlugin::GetDrawingDpi(HDC dc) const
{
    if (app_ && app_->GetAPIVersion() >= 1)
    {
        const int dpi = app_->GetDPI(drawingTaskbar ? ITrafficMonitor::DPI_TASKBAR : ITrafficMonitor::DPI_MAIN_WND);
        if (dpi > 0)
            return dpi;
    }
    // Compatibility fallback for standalone testers without host callbacks.
    return dc ? std::max(96, GetDeviceCaps(dc, LOGPIXELSX)) : 96;
}

void AwakePlugin::ShowPowerError(HWND parent)
{
    MessageBoxW(parent, manager_.GetOperationErrorText().c_str(), L"TrafficMonitor Awake", MB_OK | MB_ICONERROR);
}

void AwakePlugin::NotifyIfNeeded(bool ok)
{
    if (ok)
        return;

    HWND parent = nullptr;
    if (app_ && app_->GetAPIVersion() >= 1)
        parent = static_cast<HWND>(app_->GetMainWindowHwnd());
    ShowPowerError(parent);
}

std::wstring AwakePlugin::ResolveConfigPath() const
{
    std::wstring dir;

    if (app_)
    {
        const wchar_t* appDir = app_->GetPluginConfigDir();
        if (appDir && *appDir)
            dir = appDir;
    }

    if (dir.empty())
        dir = configDirFromExtendedInfo_;

    if (dir.empty())
    {
        wchar_t modulePath[MAX_PATH]{};
        HMODULE module = nullptr;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                   GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               kModuleAnchor, &module) &&
            GetModuleFileNameW(module, modulePath, MAX_PATH) > 0)
        {
            dir = std::filesystem::path(modulePath).parent_path().wstring();
        }
    }

    if (dir.empty())
        dir = L".";

    std::filesystem::path path(dir);
    path /= L"TrafficMonitorAwake.ini";
    return path.wstring();
}

void AwakePlugin::ShowQuickMenu(HWND hwnd, int x, int y)
{
    HMENU menu = CreatePopupMenu();
    if (!menu)
        return;

    // Keep a valid owner if a plugin tester or older host passes nullptr.
    const HWND menuOwner = hwnd ? hwnd : GetDesktopWindow();

    const auto snap = manager_.GetSnapshot();

    AppendMenuW(menu, MF_STRING | (snap.mode == AwakeManager::Mode::Off ? MF_CHECKED : 0),
                IDM_OFF, L"关闭");
    AppendMenuW(menu, MF_STRING | (snap.mode == AwakeManager::Mode::Indefinite ? MF_CHECKED : 0),
                IDM_INDEFINITE, L"无限保持唤醒");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (IsPresetChecked(snap, 30) ? MF_CHECKED : 0),
                IDM_30_MIN, L"保持 30 分钟");
    AppendMenuW(menu, MF_STRING | (IsPresetChecked(snap, 60) ? MF_CHECKED : 0),
                IDM_1_HOUR, L"保持 1 小时");
    AppendMenuW(menu, MF_STRING | (IsPresetChecked(snap, 120) ? MF_CHECKED : 0),
                IDM_2_HOURS, L"保持 2 小时");
    AppendMenuW(menu, MF_STRING | (IsPresetChecked(snap, 240) ? MF_CHECKED : 0),
                IDM_4_HOURS, L"保持 4 小时");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (snap.keepDisplayOn ? MF_CHECKED : 0),
                IDM_KEEP_DISPLAY, L"保持屏幕开启");
    AppendMenuW(menu, MF_STRING, IDM_OPTIONS, L"更多设置...");

    POINT point{x, y};
    if (hwnd)
        ClientToScreen(hwnd, &point);
    else
        GetCursorPos(&point);

    SetForegroundWindow(menuOwner);

    const UINT command = TrackPopupMenu(menu,
                                        TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
                                        point.x, point.y, 0, menuOwner, nullptr);
    DestroyMenu(menu);

    bool ok = true;
    switch (command)
    {
    case IDM_OFF:
        ok = manager_.Disable();
        break;
    case IDM_INDEFINITE:
        ok = manager_.SetIndefinite();
        break;
    case IDM_30_MIN:
        ok = manager_.SetTimed(30);
        break;
    case IDM_1_HOUR:
        ok = manager_.SetTimed(60);
        break;
    case IDM_2_HOURS:
        ok = manager_.SetTimed(120);
        break;
    case IDM_4_HOURS:
        ok = manager_.SetTimed(240);
        break;
    case IDM_KEEP_DISPLAY:
        ok = manager_.SetKeepDisplayOn(!snap.keepDisplayOn);
        break;
    case IDM_OPTIONS:
        ShowOptionsDialog(hwnd);
        return;
    default:
        return;
    }

    if (!ok)
        ShowPowerError(hwnd);
}

extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance()
{
    return &AwakePlugin::Instance();
}
