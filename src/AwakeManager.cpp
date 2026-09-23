#include "AwakeManager.h"

#include <algorithm>
#include <ctime>
#include <cwchar>
#include <iterator>

namespace
{
constexpr wchar_t kConfigSection[] = L"Awake";
constexpr wchar_t kReasonText[] = L"TrafficMonitor Awake plugin is keeping the computer awake.";

std::wstring ReadIniString(const std::wstring& path, const wchar_t* key, const wchar_t* fallback)
{
    wchar_t buffer[128]{};
    GetPrivateProfileStringW(kConfigSection, key, fallback, buffer,
                             static_cast<DWORD>(std::size(buffer)), path.c_str());
    return buffer;
}

std::uint64_t ParseUInt64(const std::wstring& value, std::uint64_t fallback)
{
    if (value.empty())
        return fallback;

    wchar_t* end = nullptr;
    const unsigned long long parsed = _wcstoui64(value.c_str(), &end, 10);
    return (end && *end == L'\0') ? static_cast<std::uint64_t>(parsed) : fallback;
}
}

AwakeManager::~AwakeManager()
{
    std::lock_guard<std::mutex> lock(mutex_);
    ClearPowerRequestLocked();
    if (powerRequest_)
    {
        CloseHandle(powerRequest_);
        powerRequest_ = nullptr;
    }
}

void AwakeManager::Initialize(const std::wstring& configPath)
{
    std::lock_guard<std::mutex> lock(mutex_);
    configPath_ = configPath;
    LoadConfigLocked();

    if ((mode_ == Mode::Timed || mode_ == Mode::Until) &&
        (expiresAtUnix_ == 0 || expiresAtUnix_ <= NowUnix()))
    {
        mode_ = Mode::Off;
        expiresAtUnix_ = 0;
        SaveConfigLocked();
    }

    ApplyPowerRequestLocked();
}

void AwakeManager::SetConfigPath(const std::wstring& configPath)
{
    if (configPath.empty())
        return;

    std::lock_guard<std::mutex> lock(mutex_);
    configPath_ = configPath;
}

bool AwakeManager::Configure(Mode mode, bool keepDisplayOn, unsigned int durationMinutes,
                             std::uint64_t untilUnix)
{
    std::lock_guard<std::mutex> lock(mutex_);

    durationMinutes = std::clamp(durationMinutes, 1u, 999u * 60u + 59u);
    const std::uint64_t now = NowUnix();

    std::uint64_t expiresAtUnix = 0;

    switch (mode)
    {
    case Mode::Off:
    case Mode::Indefinite:
        break;
    case Mode::Timed:
        expiresAtUnix = now + static_cast<std::uint64_t>(durationMinutes) * 60ull;
        break;
    case Mode::Until:
        if (untilUnix <= now)
        {
            lastError_ = ERROR_INVALID_PARAMETER;
            return false;
        }
        expiresAtUnix = untilUnix;
        break;
    default:
        lastError_ = ERROR_INVALID_PARAMETER;
        return false;
    }

    mode_ = mode;
    keepDisplayOn_ = keepDisplayOn;
    durationMinutes_ = durationMinutes;
    expiresAtUnix_ = expiresAtUnix;

    const bool ok = ApplyPowerRequestLocked();
    const bool saved = SaveConfigLocked();
    return ok && saved;
}

bool AwakeManager::Disable()
{
    const auto snap = GetSnapshot();
    return Configure(Mode::Off, snap.keepDisplayOn, snap.durationMinutes, 0);
}

bool AwakeManager::SetIndefinite()
{
    const auto snap = GetSnapshot();
    return Configure(Mode::Indefinite, snap.keepDisplayOn, snap.durationMinutes, 0);
}

bool AwakeManager::SetTimed(unsigned int minutes)
{
    const auto snap = GetSnapshot();
    return Configure(Mode::Timed, snap.keepDisplayOn, minutes, 0);
}

bool AwakeManager::SetUntil(std::uint64_t unixTime)
{
    const auto snap = GetSnapshot();
    return Configure(Mode::Until, snap.keepDisplayOn, snap.durationMinutes, unixTime);
}

bool AwakeManager::SetKeepDisplayOn(bool enabled)
{
    std::lock_guard<std::mutex> lock(mutex_);
    keepDisplayOn_ = enabled;
    const bool ok = ApplyPowerRequestLocked();
    const bool saved = SaveConfigLocked();
    return ok && saved;
}

bool AwakeManager::Toggle()
{
    const auto snap = GetSnapshot();
    return snap.mode == Mode::Off ? SetIndefinite() : Disable();
}

bool AwakeManager::Refresh()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if ((mode_ == Mode::Timed || mode_ == Mode::Until) &&
        expiresAtUnix_ != 0 && expiresAtUnix_ <= NowUnix())
    {
        mode_ = Mode::Off;
        expiresAtUnix_ = 0;
        const bool ok = ApplyPowerRequestLocked();
        const bool saved = SaveConfigLocked();
        return ok && saved;
    }
    return requestApplied_ || mode_ == Mode::Off;
}

AwakeManager::Snapshot AwakeManager::GetSnapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return Snapshot{mode_, keepDisplayOn_, expiresAtUnix_, durationMinutes_, requestApplied_, lastError_, configError_};
}

std::wstring AwakeManager::GetShortStatus() const
{
    const auto snap = GetSnapshot();
    if (snap.mode == Mode::Off)
        return L"关";

    if (!snap.requestApplied)
        return L"错误";

    if (snap.mode == Mode::Indefinite)
        return snap.keepDisplayOn ? L"开+屏" : L"开";

    const std::uint64_t now = NowUnix();
    if (snap.expiresAtUnix <= now)
        return L"关";

    return FormatRemaining(snap.expiresAtUnix - now, true);
}

std::wstring AwakeManager::GetTooltipText() const
{
    const auto snap = GetSnapshot();
    std::wstring text = L"TrafficMonitor Awake\n状态：";

    switch (snap.mode)
    {
    case Mode::Off:
        text += L"关闭";
        break;
    case Mode::Indefinite:
        text += L"无限保持唤醒";
        break;
    case Mode::Timed:
        text += L"定时保持唤醒";
        break;
    case Mode::Until:
        text += L"保持唤醒到指定时间";
        break;
    }

    if (snap.mode != Mode::Off)
    {
        text += L"\n屏幕：";
        text += snap.keepDisplayOn ? L"保持开启" : L"允许关闭";

        if (snap.mode == Mode::Timed || snap.mode == Mode::Until)
        {
            const std::uint64_t now = NowUnix();
            if (snap.expiresAtUnix > now)
            {
                text += L"\n剩余：" + FormatRemaining(snap.expiresAtUnix - now, false);
                text += L"\n结束：" + FormatLocalTime(snap.expiresAtUnix);
            }
        }

        if (!snap.requestApplied)
        {
            wchar_t errorText[64]{};
            swprintf_s(errorText, L"\n电源请求失败，错误码：%lu", snap.lastError);
            text += errorText;
        }
    }

    if (snap.configError != ERROR_SUCCESS)
        text += L"\n配置保存失败，重启后可能恢复旧设置。错误码：" + std::to_wstring(snap.configError);
    text += L"\n左键：快速开关；右键：选择模式";
    return text;
}

std::wstring AwakeManager::GetOperationErrorText() const
{
    const auto snap = GetSnapshot();
    std::wstring text;
    if (snap.lastError != ERROR_SUCCESS)
        text = L"Windows 电源请求或参数错误，错误码：" + std::to_wstring(snap.lastError) + L"。\n";
    if (snap.configError != ERROR_SUCCESS)
        text += L"配置保存失败，错误码：" + std::to_wstring(snap.configError) +
            L"。当前运行状态已更新，但重启后可能恢复旧设置；请检查配置目录权限。";
    return text;
}

std::uint64_t AwakeManager::NowUnix()
{
    return static_cast<std::uint64_t>(_time64(nullptr));
}

std::wstring AwakeManager::FormatLocalTime(std::uint64_t unixTime)
{
    __time64_t raw = static_cast<__time64_t>(unixTime);
    tm local{};
    if (_localtime64_s(&local, &raw) != 0)
        return L"未知";

    wchar_t buffer[64]{};
    wcsftime(buffer, std::size(buffer), L"%Y-%m-%d %H:%M", &local);
    return buffer;
}

std::wstring AwakeManager::FormatRemaining(std::uint64_t seconds, bool compact)
{
    // ceil(seconds / 60) without overflowing near UINT64_MAX.
    const std::uint64_t totalMinutes = seconds / 60 + (seconds % 60 != 0 ? 1 : 0);
    const std::uint64_t days = totalMinutes / (24 * 60);
    const std::uint64_t hours = (totalMinutes / 60) % 24;
    const std::uint64_t minutes = totalMinutes % 60;

    wchar_t buffer[80]{};
    if (compact)
    {
        if (days > 0)
            swprintf_s(buffer, L"%llud%02lluh", days, hours);
        else if (hours > 0)
            swprintf_s(buffer, L"%lluh%02llum", hours, minutes);
        else
            swprintf_s(buffer, L"%llum", minutes);
    }
    else
    {
        if (days > 0)
            swprintf_s(buffer, L"%llu天 %llu小时 %llu分钟", days, hours, minutes);
        else if (hours > 0)
            swprintf_s(buffer, L"%llu小时 %llu分钟", hours, minutes);
        else
            swprintf_s(buffer, L"%llu分钟", minutes);
    }
    return buffer;
}

bool AwakeManager::CreatePowerRequestLocked()
{
    if (powerRequest_)
        return true;

    REASON_CONTEXT reason{};
    reason.Version = POWER_REQUEST_CONTEXT_VERSION;
    reason.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
    reason.Reason.SimpleReasonString = const_cast<PWSTR>(kReasonText);

    powerRequest_ = PowerCreateRequest(&reason);
    if (!powerRequest_ || powerRequest_ == INVALID_HANDLE_VALUE)
    {
        lastError_ = GetLastError();
        powerRequest_ = nullptr;
        requestApplied_ = false;
        return false;
    }
    return true;
}

bool AwakeManager::ApplyPowerRequestLocked()
{
    lastError_ = ERROR_SUCCESS;
    if (!ClearPowerRequestLocked())
    {
        requestApplied_ = false;
        return false;
    }

    if (mode_ == Mode::Off)
    {
        requestApplied_ = true;
        return true;
    }

    if (!CreatePowerRequestLocked())
        return false;

    if (!PowerSetRequest(powerRequest_, PowerRequestSystemRequired))
    {
        lastError_ = GetLastError();
        requestApplied_ = false;
        return false;
    }
    systemRequestActive_ = true;

    if (keepDisplayOn_)
    {
        if (!PowerSetRequest(powerRequest_, PowerRequestDisplayRequired))
        {
            lastError_ = GetLastError();
            ClearPowerRequestLocked();
            requestApplied_ = false;
            return false;
        }
        displayRequestActive_ = true;
    }

    requestApplied_ = true;
    return true;
}

bool AwakeManager::ClearPowerRequestLocked()
{
    if (!powerRequest_)
    {
        systemRequestActive_ = false;
        displayRequestActive_ = false;
        return true;
    }

    bool ok = true;
    if (displayRequestActive_)
    {
        if (PowerClearRequest(powerRequest_, PowerRequestDisplayRequired))
            displayRequestActive_ = false;
        else
        {
            ok = false;
            if (lastError_ == ERROR_SUCCESS)
            {
                lastError_ = GetLastError();
                if (lastError_ == ERROR_SUCCESS)
                    lastError_ = ERROR_GEN_FAILURE;
            }
        }
    }
    if (systemRequestActive_)
    {
        if (PowerClearRequest(powerRequest_, PowerRequestSystemRequired))
            systemRequestActive_ = false;
        else
        {
            ok = false;
            if (lastError_ == ERROR_SUCCESS)
            {
                lastError_ = GetLastError();
                if (lastError_ == ERROR_SUCCESS)
                    lastError_ = ERROR_GEN_FAILURE;
            }
        }
    }
    return ok;
}

void AwakeManager::LoadConfigLocked()
{
    if (configPath_.empty())
        return;

    const int mode = GetPrivateProfileIntW(kConfigSection, L"Mode", 0, configPath_.c_str());
    mode_ = (mode >= 0 && mode <= 3) ? static_cast<Mode>(mode) : Mode::Off;
    keepDisplayOn_ = GetPrivateProfileIntW(kConfigSection, L"KeepDisplayOn", 0, configPath_.c_str()) != 0;

    int duration = GetPrivateProfileIntW(kConfigSection, L"DurationMinutes", 60, configPath_.c_str());
    duration = std::clamp(duration, 1, 999 * 60 + 59);
    durationMinutes_ = static_cast<unsigned int>(duration);

    expiresAtUnix_ = ParseUInt64(ReadIniString(configPath_, L"ExpiresAtUnix", L"0"), 0);
}

bool AwakeManager::SaveConfigLocked()
{
    configError_ = ERROR_SUCCESS;
    if (configPath_.empty())
    {
        configError_ = ERROR_PATH_NOT_FOUND;
        return false;
    }
    const auto write = [this](const wchar_t* key, const wchar_t* value) {
        SetLastError(ERROR_SUCCESS);
        if (!WritePrivateProfileStringW(kConfigSection, key, value, configPath_.c_str()) &&
            configError_ == ERROR_SUCCESS)
        {
            configError_ = GetLastError();
            if (configError_ == ERROR_SUCCESS)
                configError_ = ERROR_WRITE_FAULT;
        }
    };

    wchar_t buffer[64]{};

    swprintf_s(buffer, L"%d", static_cast<int>(mode_));
    write(L"Mode", buffer);

    write(L"KeepDisplayOn", keepDisplayOn_ ? L"1" : L"0");

    swprintf_s(buffer, L"%u", durationMinutes_);
    write(L"DurationMinutes", buffer);

    swprintf_s(buffer, L"%llu", expiresAtUnix_);
    write(L"ExpiresAtUnix", buffer);
    return configError_ == ERROR_SUCCESS;
}
