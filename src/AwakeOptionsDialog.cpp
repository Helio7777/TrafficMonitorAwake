#include "AwakeOptionsDialog.h"
#include "AwakeManager.h"

#include <commctrl.h>
#include <algorithm>
#include <ctime>
#include <cwchar>
#include <iterator>

namespace
{
constexpr wchar_t kWindowClass[] = L"TrafficMonitorAwakeOptionsWindow";
constexpr wchar_t kWindowTitle[] = L"TrafficMonitor Awake 设置";

constexpr int IDC_MODE = 1001;
constexpr int IDC_KEEP_DISPLAY = 1002;
constexpr int IDC_HOURS = 1003;
constexpr int IDC_MINUTES = 1004;
constexpr int IDC_DATE = 1005;
constexpr int IDC_TIME = 1006;

HINSTANCE ThisModule()
{
    HMODULE module = nullptr;
    if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           kWindowClass, &module))
    {
        return static_cast<HINSTANCE>(module);
    }
    return nullptr;
}

void SetControlFont(HWND hwnd)
{
    SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
}

HWND AddControl(HWND parent, const wchar_t* cls, const wchar_t* text, DWORD style,
                int x, int y, int w, int h, int id)
{
    HWND control = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                                   x, y, w, h, parent,
                                   reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                   ThisModule(), nullptr);
    if (control)
        SetControlFont(control);
    return control;
}

unsigned int ReadUnsignedEdit(HWND hwnd, unsigned int fallback)
{
    wchar_t text[32]{};
    GetWindowTextW(hwnd, text, static_cast<int>(std::size(text)));
    wchar_t* end = nullptr;
    const unsigned long value = wcstoul(text, &end, 10);
    if (!end || *end != L'\0')
        return fallback;
    return static_cast<unsigned int>(value);
}
}

AwakeOptionsDialog::AwakeOptionsDialog(AwakeManager& manager)
    : manager_(manager)
{
}

bool AwakeOptionsDialog::Show(HWND parent, AwakeManager& manager)
{
    AwakeOptionsDialog dialog(manager);
    return dialog.Run(parent);
}

bool AwakeOptionsDialog::Run(HWND parent)
{
    INITCOMMONCONTROLSEX controls{};
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_DATE_CLASSES;
    InitCommonControlsEx(&controls);

    parent_ = parent;
    if (!RegisterWindowClass() || !Create(parent))
    {
        if (classRegistered_)
            UnregisterClassW(kWindowClass, ThisModule());
        return false;
    }

    const bool restoreParent = parent_ && IsWindowEnabled(parent_);
    if (restoreParent)
        EnableWindow(parent_, FALSE);

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    MSG msg{};
    bool quitReceived = false;
    while (IsWindow(hwnd_))
    {
        const BOOL result = GetMessageW(&msg, nullptr, 0, 0);
        if (result <= 0)
        {
            quitReceived = result == 0;
            break;
        }
        if (!IsDialogMessageW(hwnd_, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    // Destroy before the stack-owned dialog and its GWLP_USERDATA expire.
    if (IsWindow(hwnd_))
        DestroyWindow(hwnd_);

    if (restoreParent && IsWindow(parent_))
    {
        EnableWindow(parent_, TRUE);
        SetActiveWindow(parent_);
    }

    if (classRegistered_)
    {
        UnregisterClassW(kWindowClass, ThisModule());
        classRegistered_ = false;
    }

    if (quitReceived)
        PostQuitMessage(static_cast<int>(msg.wParam));
    return accepted_;
}

bool AwakeOptionsDialog::RegisterWindowClass()
{
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = &AwakeOptionsDialog::WindowProc;
    wc.hInstance = ThisModule();
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kWindowClass;

    if (RegisterClassExW(&wc))
    {
        classRegistered_ = true;
        return true;
    }

    return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}

bool AwakeOptionsDialog::Create(HWND parent)
{
    hwnd_ = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        kWindowClass,
        kWindowTitle,
        WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 470, 325,
        parent, nullptr, ThisModule(), this);

    if (!hwnd_)
        return false;

    CenterToParent();
    return true;
}

void AwakeOptionsDialog::CreateControls()
{
    AddControl(hwnd_, L"STATIC", L"唤醒模式：", 0, 24, 22, 100, 22, -1);
    modeCombo_ = AddControl(hwnd_, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP,
                            125, 18, 300, 180, IDC_MODE);

    SendMessageW(modeCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"关闭"));
    SendMessageW(modeCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"无限保持唤醒"));
    SendMessageW(modeCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"保持一段时间"));
    SendMessageW(modeCombo_, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"保持到指定时间"));

    keepDisplayCheck_ = AddControl(hwnd_, L"BUTTON", L"保持屏幕开启",
                                   BS_AUTOCHECKBOX | WS_TABSTOP,
                                   125, 58, 180, 24, IDC_KEEP_DISPLAY);

    AddControl(hwnd_, L"STATIC", L"持续时间：", 0, 24, 102, 100, 22, -1);
    hoursEdit_ = AddControl(hwnd_, L"EDIT", L"1", WS_BORDER | ES_NUMBER | ES_RIGHT | WS_TABSTOP,
                            125, 98, 62, 24, IDC_HOURS);
    AddControl(hwnd_, L"STATIC", L"小时", 0, 194, 102, 45, 22, -1);
    minutesEdit_ = AddControl(hwnd_, L"EDIT", L"0", WS_BORDER | ES_NUMBER | ES_RIGHT | WS_TABSTOP,
                              246, 98, 62, 24, IDC_MINUTES);
    AddControl(hwnd_, L"STATIC", L"分钟", 0, 315, 102, 45, 22, -1);

    AddControl(hwnd_, L"STATIC", L"结束时间：", 0, 24, 146, 100, 22, -1);
    datePicker_ = AddControl(hwnd_, DATETIMEPICK_CLASSW, L"",
                             DTS_SHORTDATEFORMAT | WS_TABSTOP,
                             125, 142, 145, 26, IDC_DATE);
    timePicker_ = AddControl(hwnd_, DATETIMEPICK_CLASSW, L"",
                             DTS_TIMEFORMAT | WS_TABSTOP,
                             280, 142, 145, 26, IDC_TIME);

    AddControl(hwnd_, L"STATIC",
               L"说明：保持屏幕开启会同时阻止系统休眠；关闭该选项时仅保持系统运行，允许显示器按系统设置关闭。",
               SS_LEFT, 24, 188, 401, 46, -1);

    AddControl(hwnd_, L"BUTTON", L"确定", BS_DEFPUSHBUTTON | WS_TABSTOP,
               247, 250, 82, 28, IDOK);
    AddControl(hwnd_, L"BUTTON", L"取消", WS_TABSTOP,
               343, 250, 82, 28, IDCANCEL);
}

void AwakeOptionsDialog::InitializeControls()
{
    const auto snap = manager_.GetSnapshot();
    SendMessageW(modeCombo_, CB_SETCURSEL, static_cast<WPARAM>(snap.mode), 0);
    SendMessageW(keepDisplayCheck_, BM_SETCHECK, snap.keepDisplayOn ? BST_CHECKED : BST_UNCHECKED, 0);

    wchar_t buffer[32]{};
    const unsigned int hours = snap.durationMinutes / 60;
    const unsigned int minutes = snap.durationMinutes % 60;

    swprintf_s(buffer, L"%u", hours);
    SetWindowTextW(hoursEdit_, buffer);
    swprintf_s(buffer, L"%u", minutes);
    SetWindowTextW(minutesEdit_, buffer);

    std::uint64_t until = snap.expiresAtUnix;
    const std::uint64_t now = static_cast<std::uint64_t>(_time64(nullptr));
    if (until <= now)
        until = now + 3600;
    SetPickerTime(datePicker_, timePicker_, until);

    UpdateControlState();
}

void AwakeOptionsDialog::UpdateControlState()
{
    const int mode = static_cast<int>(SendMessageW(modeCombo_, CB_GETCURSEL, 0, 0));
    const bool timed = mode == static_cast<int>(AwakeManager::Mode::Timed);
    const bool until = mode == static_cast<int>(AwakeManager::Mode::Until);
    const bool enabled = mode != static_cast<int>(AwakeManager::Mode::Off);

    EnableWindow(keepDisplayCheck_, enabled);
    EnableWindow(hoursEdit_, timed);
    EnableWindow(minutesEdit_, timed);
    EnableWindow(datePicker_, until);
    EnableWindow(timePicker_, until);
}

bool AwakeOptionsDialog::Apply()
{
    const int modeIndex = static_cast<int>(SendMessageW(modeCombo_, CB_GETCURSEL, 0, 0));
    if (modeIndex < 0 || modeIndex > 3)
        return false;

    const auto mode = static_cast<AwakeManager::Mode>(modeIndex);
    const bool keepDisplay = SendMessageW(keepDisplayCheck_, BM_GETCHECK, 0, 0) == BST_CHECKED;

    unsigned int hours = std::min(ReadUnsignedEdit(hoursEdit_, 0), 999u);
    unsigned int minutes = std::min(ReadUnsignedEdit(minutesEdit_, 0), 59u);
    unsigned int durationMinutes = hours * 60u + minutes;
    if (durationMinutes == 0)
        durationMinutes = 1;

    std::uint64_t until = 0;
    if (mode == AwakeManager::Mode::Until)
    {
        until = ReadUntilTime(datePicker_, timePicker_);
        const std::uint64_t now = static_cast<std::uint64_t>(_time64(nullptr));
        if (until <= now)
        {
            MessageBoxW(hwnd_, L"结束时间必须晚于当前时间。", L"TrafficMonitor Awake",
                        MB_OK | MB_ICONWARNING);
            return false;
        }
    }

    if (!manager_.Configure(mode, keepDisplay, durationMinutes, until))
    {
        MessageBoxW(hwnd_, manager_.GetOperationErrorText().c_str(), L"TrafficMonitor Awake", MB_OK | MB_ICONERROR);
        return false;
    }

    return true;
}

void AwakeOptionsDialog::CenterToParent()
{
    RECT windowRect{};
    GetWindowRect(hwnd_, &windowRect);
    const int width = windowRect.right - windowRect.left;
    const int height = windowRect.bottom - windowRect.top;

    RECT ref{};
    if (parent_ && GetWindowRect(parent_, &ref))
    {
        const int x = ref.left + ((ref.right - ref.left) - width) / 2;
        const int y = ref.top + ((ref.bottom - ref.top) - height) / 2;
        SetWindowPos(hwnd_, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
    else
    {
        const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
        const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
        SetWindowPos(hwnd_, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }
}

LRESULT CALLBACK AwakeOptionsDialog::WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    AwakeOptionsDialog* self = reinterpret_cast<AwakeOptionsDialog*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    if (msg == WM_NCCREATE)
    {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<AwakeOptionsDialog*>(create->lpCreateParams);
        self->hwnd_ = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    if (self)
    {
        if (msg == WM_NCDESTROY)
        {
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            self->hwnd_ = nullptr;
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
        return self->HandleMessage(msg, wParam, lParam);
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT AwakeOptionsDialog::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        CreateControls();
        InitializeControls();
        return 0;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_MODE && HIWORD(wParam) == CBN_SELCHANGE)
        {
            UpdateControlState();
            return 0;
        }
        if (LOWORD(wParam) == IDOK)
        {
            if (Apply())
            {
                accepted_ = true;
                DestroyWindow(hwnd_);
            }
            return 0;
        }
        if (LOWORD(wParam) == IDCANCEL)
        {
            accepted_ = false;
            DestroyWindow(hwnd_);
            return 0;
        }
        break;

    case WM_CLOSE:
        accepted_ = false;
        DestroyWindow(hwnd_);
        return 0;

    case WM_DESTROY:
        return 0;
    }

    return DefWindowProcW(hwnd_, msg, wParam, lParam);
}

std::uint64_t AwakeOptionsDialog::ReadUntilTime(HWND datePicker, HWND timePicker)
{
    SYSTEMTIME date{};
    SYSTEMTIME time{};
    if (DateTime_GetSystemtime(datePicker, &date) != GDT_VALID ||
        DateTime_GetSystemtime(timePicker, &time) != GDT_VALID)
    {
        return 0;
    }

    tm local{};
    local.tm_year = static_cast<int>(date.wYear) - 1900;
    local.tm_mon = static_cast<int>(date.wMonth) - 1;
    local.tm_mday = static_cast<int>(date.wDay);
    local.tm_hour = static_cast<int>(time.wHour);
    local.tm_min = static_cast<int>(time.wMinute);
    local.tm_sec = 0;
    local.tm_isdst = -1;

    const __time64_t result = _mktime64(&local);
    return result > 0 ? static_cast<std::uint64_t>(result) : 0;
}

void AwakeOptionsDialog::SetPickerTime(HWND datePicker, HWND timePicker, std::uint64_t unixTime)
{
    __time64_t raw = static_cast<__time64_t>(unixTime);
    tm local{};
    if (_localtime64_s(&local, &raw) != 0)
        return;

    SYSTEMTIME st{};
    st.wYear = static_cast<WORD>(local.tm_year + 1900);
    st.wMonth = static_cast<WORD>(local.tm_mon + 1);
    st.wDay = static_cast<WORD>(local.tm_mday);
    st.wHour = static_cast<WORD>(local.tm_hour);
    st.wMinute = static_cast<WORD>(local.tm_min);
    st.wSecond = 0;

    DateTime_SetSystemtime(datePicker, GDT_VALID, &st);
    DateTime_SetSystemtime(timePicker, GDT_VALID, &st);
}
