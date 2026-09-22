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
    if (snap.mode == AwakeManager::Mode::Off)
        return darkMode ? RGB(100, 116, 139) : RGB(71, 85, 105);
    if (!snap.requestApplied)
        return RGB(239, 68, 68);
    if (snap.mode == AwakeManager::Mode::Indefinite)
        return RGB(16, 185, 129);
    return RGB(245, 158, 11);
}

void DrawFlatBox(Graphics& graphics, const RectF& rect, REAL radius,
                 const Color& fill, const Color& border, REAL stroke)
{
    const REAL diameter = radius * 2.0f;
    GraphicsPath path;
    path.AddArc(rect.X, rect.Y, diameter, diameter, 180.0f, 90.0f);
    path.AddArc(rect.GetRight() - diameter, rect.Y, diameter, diameter, 270.0f, 90.0f);
    path.AddArc(rect.GetRight() - diameter, rect.GetBottom() - diameter,
                diameter, diameter, 0.0f, 90.0f);
    path.AddArc(rect.X, rect.GetBottom() - diameter, diameter, diameter, 90.0f, 90.0f);
    path.CloseFigure();
    SolidBrush brush(fill);
    Pen pen(border, stroke);
    graphics.FillPath(&brush, &path);
    graphics.DrawPath(&pen, &path);
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
    const bool tinyRow = h <= 18.0f * scale;
    const float edge = (tinyRow ? 0.5f : 1.5f) * scale;
    const REAL capsuleHeight = std::max(7.0f * scale,
        std::min(static_cast<REAL>(h) - edge * 2.0f,
                 (doubleLine ? 32.0f : 20.0f) * scale));
    const REAL capsuleWidth = std::min(static_cast<REAL>(w) - edge * 2.0f,
        std::max(34.0f * scale, capsuleHeight * 1.55f));
    // Snap the shared center once. Capsule, glyph and composite display mark
    // must all use exactly the same center to avoid a one-pixel optical drift.
    const REAL cellCenterX = std::floor((static_cast<REAL>(x) + w * 0.5f) * 2.0f) / 2.0f;
    const RectF capsule(cellCenterX - capsuleWidth / 2.0f,
                        y + (h - capsuleHeight) / 2.0f,
                        capsuleWidth, capsuleHeight);
    const float size = std::min({ capsuleHeight - 4.0f * scale,
                                  (doubleLine ? 24.0f : 18.0f) * scale,
                                  capsuleWidth * (doubleLine ? 0.56f : 0.46f) });
    if (size < 7)
        return;

    const bool hasDisplayBadge = snap.keepDisplayOn && snap.requestApplied && snap.mode != AwakeManager::Mode::Off;
    const REAL statusCenter = cellCenterX;
    const REAL left = statusCenter - size / 2.0f;
    const REAL top = capsule.Y + (capsule.Height - size) / 2.0f;
    const REAL center = size / 2.0f;
    const COLORREF rgb = StatusColor(snap, darkMode);
    const Color status(255, GetRValue(rgb), GetGValue(rgb), GetBValue(rgb));
    Graphics graphics(hdc);
    graphics.SetSmoothingMode(SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(PixelOffsetModeHalf);
    graphics.SetCompositingMode(CompositingModeSourceOver);
    graphics.SetClip(Rect(x, y, w, h), CombineModeIntersect);

    const float stroke = (tinyRow ? 1.9f : 1.8f) * scale;
    const bool filledBadge = snap.mode != AwakeManager::Mode::Off;
    const Color flatFill(filledBadge ? 220 : 38,
                         GetRValue(rgb), GetGValue(rgb), GetBValue(rgb));
    DrawFlatBox(graphics, capsule, capsule.Height / 2.0f,
                flatFill, status, std::max(1.0f, scale));
    Pen outline(filledBadge ? Color(255, 255, 255, 255) : status, stroke);
    outline.SetStartCap(LineCapRound);
    outline.SetEndCap(LineCapRound);
    outline.SetLineJoin(LineJoinRound);

    // One recognizable silhouette, with no nested rings at small sizes.
    const Color glyphColor = hasDisplayBadge
        ? Color(255, 250, 190, 32)
        : (filledBadge ? Color(255, 255, 255, 255) : status);
    Pen glyph(glyphColor, stroke);
    glyph.SetStartCap(LineCapRound);
    glyph.SetEndCap(LineCapRound);
    glyph.SetLineJoin(LineJoinRound);
    REAL cx = static_cast<REAL>(left + center);
    const REAL cy = static_cast<REAL>(top + center);
    const REAL inset = stroke / 2.0f + scale;

    if (hasDisplayBadge)
    {
        // Composite “display-on” glyph: a monitor outline containing a
        // power mark. It communicates one feature without a second badge.
        const REAL monitorLeft = left + size * 0.16f;
        const REAL monitorTop = top + size * 0.16f;
        const REAL monitorWidth = size * 0.68f;
        const REAL monitorHeight = size * 0.48f;
        graphics.DrawRectangle(&glyph, monitorLeft, monitorTop,
                               monitorWidth, monitorHeight);
        graphics.DrawLine(&glyph, monitorLeft + monitorWidth / 2.0f,
                          monitorTop + monitorHeight,
                          monitorLeft + monitorWidth / 2.0f,
                          monitorTop + monitorHeight + size * 0.14f);
        graphics.DrawLine(&glyph, monitorLeft + monitorWidth * 0.30f,
                          monitorTop + monitorHeight + size * 0.14f,
                          monitorLeft + monitorWidth * 0.70f,
                          monitorTop + monitorHeight + size * 0.14f);
        const REAL markCenterX = cellCenterX;
        graphics.DrawLine(&glyph, markCenterX, top + size * 0.28f,
                          markCenterX, top + size * 0.48f);
        const RectF powerArc(left + size * 0.30f, top + size * 0.25f,
                             size * 0.40f, size * 0.40f);
        graphics.DrawArc(&glyph, powerArc, 42.0f, 276.0f);
    }
    else if (!snap.requestApplied && snap.mode != AwakeManager::Mode::Off)
    {
        graphics.DrawEllipse(&outline, left + inset, top + inset,
                             size - inset * 2.0f, size - inset * 2.0f);
        graphics.DrawLine(&glyph, cx, top + size * 0.30f,
                          cx, top + size * 0.54f);
        SolidBrush dot(glyphColor);
        const REAL d = stroke / 2.0f;
        graphics.FillEllipse(&dot, cx - d, top + size * 0.72f - d,
                             d * 2.0f, d * 2.0f);
    }
    else if (snap.mode == AwakeManager::Mode::Timed || snap.mode == AwakeManager::Mode::Until)
    {
        graphics.DrawEllipse(&glyph, static_cast<REAL>(left) + inset,
                             static_cast<REAL>(top) + inset,
                             static_cast<REAL>(size) - inset * 2.0f,
                             static_cast<REAL>(size) - inset * 2.0f);
        graphics.DrawLine(&glyph, cx, cy, cx, top + size * 0.28f);
        graphics.DrawLine(&glyph, cx, cy, left + size * 0.70f, cy + size * 0.10f);
    }
    else
    {
        graphics.DrawLine(&glyph, cx, top + inset, cx, top + size * 0.48f);
        const RectF arcRect(static_cast<REAL>(left) + inset,
                            static_cast<REAL>(top) + inset,
                            static_cast<REAL>(size) - inset * 2.0f,
                            static_cast<REAL>(size) - inset * 2.0f);
        graphics.DrawArc(&glyph, arcRect, -48.0f, 276.0f);
    }

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
        return L"1.0.0";
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
