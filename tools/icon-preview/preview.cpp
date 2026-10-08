// Render the production implementation directly so previews cannot drift.
#include "../../src/AwakePlugin.cpp"
#include <iostream>

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2)
        return 1;
    GdiplusStartupInput input;
    ULONG_PTR token{};
    if (GdiplusStartup(&token, &input, nullptr) != Ok)
        return 2;
    HDC dc = CreateCompatibleDC(nullptr);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = 940;
    info.bmiHeader.biHeight = -1240;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr;
    HBITMAP dib = dc ? CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0) : nullptr;
    if (!dib)
    {
        if (dc)
            DeleteDC(dc);
        GdiplusShutdown(token);
        return 4;
    }
    HGDIOBJ previous = SelectObject(dc, dib);
    int result = 0;
    {
        // Use an opaque Win32 surface like TrafficMonitor. Graphics::GetHDC()
        // on a GDI+ bitmap introduces black matte fringes in mixed rendering.
        for (int theme = 0; theme < 2; ++theme)
        {
            RECT background{0, theme * 620, 940, (theme + 1) * 620};
            HBRUSH brush = CreateSolidBrush(theme ? RGB(25, 30, 38) : RGB(245, 247, 250));
            FillRect(dc, &background, brush);
            DeleteObject(brush);
        }
        SetBkMode(dc, TRANSPARENT);
        for (int theme = 0; theme < 2; ++theme)
        {
            SetTextColor(dc, theme ? RGB(220, 225, 235) : RGB(35, 45, 60));
            const wchar_t* labels[] = {L"Off", L"Awake", L"Timer", L"Error", L"Display on", L"Error+display", L"Timed screen", L"Off error"};
            for (int state = 0; state < 8; ++state)
                TextOutW(dc, 160 + state * 92, theme * 620 + 12, labels[state], lstrlenW(labels[state]));
            for (int row = 0; row < 8; ++row)
            {
                const int dpis[] = {96, 96, 120, 144, 144, 192, 192, 96};
                const int heights[] = {18, 34, 23, 27, 51, 36, 68, 14};
                const wchar_t* names[] = {L"1 row / 100%", L"2 rows / 100%", L"Odd cell / 125%", L"1 row / 150%", L"2 rows / 150%", L"1 row / 200%", L"2 rows / 200%", L"Wide / 14px row"};
                const int width = row == 7 ? 79 :
                    MulDiv(AwakePlugin::Instance().GetItem(0)->GetItemWidth(), dpis[row], 96) + (row == 2 ? 1 : 0);
                const int y = theme * 620 + 38 + row * 72;
                TextOutW(dc, 12, y + 8, names[row], lstrlenW(names[row]));
                for (int state = 0; state < 8; ++state)
                {
                    AwakeManager::Snapshot snap;
                    snap.mode = state == 0 || state == 7 ? AwakeManager::Mode::Off :
                        state == 2 || state == 6 ? AwakeManager::Mode::Timed : AwakeManager::Mode::Indefinite;
                    snap.requestApplied = state != 3 && state != 5 && state != 7;
                    snap.lastError = snap.requestApplied ? ERROR_SUCCESS : ERROR_GEN_FAILURE;
                    snap.keepDisplayOn = state >= 4 && state <= 6;
                    DrawStatusIcon(dc, 164 + state * 92, y, width, heights[row], snap, theme != 0, dpis[row]);
                }
            }
        }
        Bitmap bitmap(dib, nullptr);
        const CLSID pngEncoder = {0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
        if (bitmap.Save(argv[1], &pngEncoder, nullptr) != Ok)
            result = 3;
    }
    SelectObject(dc, previous);
    DeleteObject(dib);
    DeleteDC(dc);
    GdiplusShutdown(token);
    return result;
}
