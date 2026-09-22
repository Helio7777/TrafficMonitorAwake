#pragma once

// Minimal ABI-compatible copy of the current TrafficMonitor plugin interface
// definitions required by this plugin. Method order/signatures match API v8.

class IPluginDrawer;

class IPluginItem
{
public:
    virtual const wchar_t* GetItemName() const = 0;
    virtual const wchar_t* GetItemId() const = 0;
    virtual const wchar_t* GetItemLableText() const = 0;
    virtual const wchar_t* GetItemValueText() const = 0;
    virtual const wchar_t* GetItemValueSampleText() const = 0;
    virtual bool IsCustomDraw() const { return false; }
    virtual int GetItemWidth() const { return 0; }
    virtual void DrawItem(void*, int, int, int, int, bool) {}
    virtual int GetItemWidthEx(void*) const { return 0; }

    enum MouseEventType
    {
        MT_LCLICKED,
        MT_RCLICKED,
        MT_DBCLICKED,
        MT_WHEEL_UP,
        MT_WHEEL_DOWN,
    };

    enum MouseEventFlag
    {
        MF_TASKBAR_WND = 1 << 0,
    };

    virtual int OnMouseEvent(MouseEventType, int, int, void*, int) { return 0; }

    enum KeyboardEventFlag
    {
        KF_TASKBAR_WND = 1 << 0,
    };

    virtual int OnKeboardEvent(int, bool, bool, bool, void*, int) { return 0; }

    enum ItemInfoType
    {
    };

    virtual void* OnItemInfo(ItemInfoType, void*, void*) { return 0; }
    virtual int IsDrawResourceUsageGraph() const { return 0; }
    virtual float GetResourceUsageGraphValue() const { return 0.0f; }
    virtual bool DrawItemEx(IPluginDrawer*, int, int, int, int, bool) { return false; }
    virtual int IsDoubleLineExclusive() const { return 0; }
};

class ITrafficMonitor;

class ITMPlugin
{
public:
    virtual int GetAPIVersion() const { return 8; }
    virtual IPluginItem* GetItem(int index) = 0;
    virtual void DataRequired() = 0;

    enum OptionReturn
    {
        OR_OPTION_CHANGED,
        OR_OPTION_UNCHANGED,
        OR_OPTION_NOT_PROVIDED,
    };

    virtual OptionReturn ShowOptionsDialog(void*) { return OR_OPTION_NOT_PROVIDED; }

    enum PluginInfoIndex
    {
        TMI_NAME,
        TMI_DESCRIPTION,
        TMI_AUTHOR,
        TMI_COPYRIGHT,
        TMI_VERSION,
        TMI_URL,
        TMI_MAX,
    };

    virtual const wchar_t* GetInfo(PluginInfoIndex index) = 0;

    struct MonitorInfo
    {
        unsigned long long up_speed{};
        unsigned long long down_speed{};
        int cpu_usage{};
        int memory_usage{};
        int gpu_usage{};
        int hdd_usage{};
        int cpu_temperature{};
        int gpu_temperature{};
        int hdd_temperature{};
        int main_board_temperature{};
        int cpu_freq{};
    };

    virtual void OnMonitorInfo(const MonitorInfo&) {}
    virtual const wchar_t* GetTooltipInfo() { return L""; }

    enum ExtendedInfoIndex
    {
        EI_LABEL_TEXT_COLOR,
        EI_VALUE_TEXT_COLOR,
        EI_DRAW_TASKBAR_WND,
        EI_NAIN_WND_NET_SPEED_SHORT_MODE,
        EI_MAIN_WND_SPERATE_WITH_SPACE,
        EI_MAIN_WND_UNIT_BYTE,
        EI_MAIN_WND_UNIT_SELECT,
        EI_MAIN_WND_NOT_SHOW_UNIT,
        EI_MAIN_WND_NOT_SHOW_PERCENT,
        EI_TASKBAR_WND_NET_SPEED_SHORT_MODE,
        EI_TASKBAR_WND_SPERATE_WITH_SPACE,
        EI_TASKBAR_WND_VALUE_RIGHT_ALIGN,
        EI_TASKBAR_WND_NET_SPEED_WIDTH,
        EI_TASKBAR_WND_UNIT_BYTE,
        EI_TASKBAR_WND_UNIT_SELECT,
        EI_TASKBAR_WND_NOT_SHOW_UNIT,
        EI_TASKBAR_WND_NOT_SHOW_PERCENT,
        EI_CONFIG_DIR,
    };

    virtual void OnExtenedInfo(ExtendedInfoIndex, const wchar_t*) {}
    virtual void* GetPluginIcon() { return nullptr; }
    virtual int GetCommandCount() { return 0; }
    virtual const wchar_t* GetCommandName(int) { return nullptr; }
    virtual void* GetCommandIcon(int) { return nullptr; }
    virtual void OnPluginCommand(int, void*, void*) {}
    virtual int IsCommandChecked(int) { return false; }
    virtual void OnInitialize(ITrafficMonitor*) {}
};

class ITrafficMonitor
{
public:
    virtual int GetAPIVersion() = 0;
    virtual const wchar_t* GetVersion() = 0;

    enum MonitorItem
    {
        MI_UP,
        MI_DOWN,
        MI_CPU,
        MI_MEMORY,
        MI_GPU_USAGE,
        MI_CPU_TEMP,
        MI_GPU_TEMP,
        MI_HDD_TEMP,
        MI_MAIN_BOARD_TEMP,
        MI_HDD_USAGE,
        MI_CPU_FREQ,
        MI_TODAY_UP_TRAFFIC,
        MI_TODAY_DOWN_TRAFFIC,
    };

    virtual double GetMonitorValue(MonitorItem) = 0;
    virtual const wchar_t* GetMonitorValueString(MonitorItem, int is_main_window = false) = 0;
    virtual void ShowNotifyMessage(const wchar_t*) = 0;
    virtual unsigned short GetLanguageId() const = 0;
    virtual const wchar_t* GetPluginConfigDir() const = 0;

    enum DPIType
    {
        DPI_MAIN_WND,
        DPI_TASKBAR,
    };

    virtual int GetDPI(DPIType) const = 0;
    virtual unsigned int GetThemeColor() const = 0;
    virtual const wchar_t* GetStringRes(const wchar_t*, const wchar_t*) = 0;
    virtual void* GetMainWindowHwnd() = 0;
    virtual void* GetTaskbarWindowHwnd() = 0;
};
