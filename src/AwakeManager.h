#pragma once

#include <windows.h>
#include <cstdint>
#include <mutex>
#include <string>

class AwakeManager
{
public:
    enum class Mode : int
    {
        Off = 0,
        Indefinite = 1,
        Timed = 2,
        Until = 3,
    };

    struct Snapshot
    {
        Mode mode{Mode::Off};
        bool keepDisplayOn{false};
        std::uint64_t expiresAtUnix{0};
        unsigned int durationMinutes{60};
        bool requestApplied{false};
        DWORD lastError{ERROR_SUCCESS};
        DWORD configError{ERROR_SUCCESS};
    };

    AwakeManager() = default;
    ~AwakeManager();

    AwakeManager(const AwakeManager&) = delete;
    AwakeManager& operator=(const AwakeManager&) = delete;

    void Initialize(const std::wstring& configPath);
    void SetConfigPath(const std::wstring& configPath);

    bool Configure(Mode mode, bool keepDisplayOn, unsigned int durationMinutes,
                   std::uint64_t untilUnix = 0);
    bool Disable();
    bool SetIndefinite();
    bool SetTimed(unsigned int minutes);
    bool SetUntil(std::uint64_t unixTime);
    bool SetKeepDisplayOn(bool enabled);
    bool Toggle();

    // Called from TrafficMonitor::DataRequired. Also expires timed modes.
    bool Refresh();

    Snapshot GetSnapshot() const;
    std::wstring GetShortStatus() const;
    std::wstring GetTooltipText() const;
    std::wstring GetOperationErrorText() const;

private:
    static std::uint64_t NowUnix();
    static std::wstring FormatLocalTime(std::uint64_t unixTime);
    static std::wstring FormatRemaining(std::uint64_t seconds, bool compact);

    bool CreatePowerRequestLocked();
    bool ApplyPowerRequestLocked();
    bool ClearPowerRequestLocked();
    void LoadConfigLocked();
    bool SaveConfigLocked();

private:
    mutable std::mutex mutex_;
    std::wstring configPath_;
    Mode mode_{Mode::Off};
    bool keepDisplayOn_{false};
    std::uint64_t expiresAtUnix_{0};
    unsigned int durationMinutes_{60};

    HANDLE powerRequest_{nullptr};
    bool systemRequestActive_{false};
    bool displayRequestActive_{false};
    bool requestApplied_{false};
    DWORD lastError_{ERROR_SUCCESS};
    DWORD configError_{ERROR_SUCCESS};
};
