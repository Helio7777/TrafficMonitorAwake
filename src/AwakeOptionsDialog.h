#pragma once

#include <windows.h>
#include <cstdint>

class AwakeManager;

class AwakeOptionsDialog
{
public:
    static bool Show(HWND parent, AwakeManager& manager);

private:
    explicit AwakeOptionsDialog(AwakeManager& manager);

    bool Run(HWND parent);
    bool RegisterWindowClass();
    bool Create(HWND parent);
    void CreateControls();
    void InitializeControls();
    void UpdateControlState();
    bool Apply();
    void CenterToParent();

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

    static std::uint64_t ReadUntilTime(HWND datePicker, HWND timePicker);
    static void SetPickerTime(HWND datePicker, HWND timePicker, std::uint64_t unixTime);

private:
    AwakeManager& manager_;
    HWND hwnd_{nullptr};
    HWND parent_{nullptr};
    HWND modeCombo_{nullptr};
    HWND keepDisplayCheck_{nullptr};
    HWND hoursEdit_{nullptr};
    HWND minutesEdit_{nullptr};
    HWND datePicker_{nullptr};
    HWND timePicker_{nullptr};
    bool accepted_{false};
    bool classRegistered_{false};
};
