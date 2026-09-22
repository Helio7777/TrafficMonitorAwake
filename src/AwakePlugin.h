#pragma once

#include "PluginInterface.h"
#include "AwakeManager.h"

#include <string>

class AwakePlugin;

class AwakeItem final : public IPluginItem
{
public:
    explicit AwakeItem(AwakePlugin& owner) : owner_(owner) {}

    const wchar_t* GetItemName() const override;
    const wchar_t* GetItemId() const override;
    const wchar_t* GetItemLableText() const override;
    const wchar_t* GetItemValueText() const override;
    const wchar_t* GetItemValueSampleText() const override;
    bool IsCustomDraw() const override;
    int GetItemWidth() const override;
    void DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode) override;
    int IsDoubleLineExclusive() const override;
    int OnMouseEvent(MouseEventType type, int x, int y, void* hWnd, int flag) override;

    void SetValueText(std::wstring value);

private:
    AwakePlugin& owner_;
    std::wstring valueText_{L"关"};
};

class AwakePlugin final : public ITMPlugin
{
public:
    AwakePlugin();

    static AwakePlugin& Instance();

    IPluginItem* GetItem(int index) override;
    void DataRequired() override;
    OptionReturn ShowOptionsDialog(void* hParent) override;
    const wchar_t* GetInfo(PluginInfoIndex index) override;
    const wchar_t* GetTooltipInfo() override;
    void OnExtenedInfo(ExtendedInfoIndex index, const wchar_t* data) override;
    int GetCommandCount() override;
    const wchar_t* GetCommandName(int command_index) override;
    void OnPluginCommand(int command_index, void* hWnd, void* para) override;
    int IsCommandChecked(int command_index) override;
    void OnInitialize(ITrafficMonitor* pApp) override;

    int HandleItemMouse(IPluginItem::MouseEventType type, int x, int y, HWND hwnd, int flag);
    AwakeManager& Manager() { return manager_; }
    int GetDrawingDpi(HDC dc) const;

private:
    void ShowPowerError(HWND parent);
    void NotifyIfNeeded(bool ok);
    std::wstring ResolveConfigPath() const;
    void ShowQuickMenu(HWND hwnd, int x, int y);

private:
    AwakeManager manager_;
    AwakeItem item_;
    ITrafficMonitor* app_{nullptr};
    std::wstring configDirFromExtendedInfo_;
    std::wstring tooltip_;
};
