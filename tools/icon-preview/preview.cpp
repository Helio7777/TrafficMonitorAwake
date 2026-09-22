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
    int result = 0;
    {
        Bitmap bitmap(740, 1000, PixelFormat32bppARGB);
        Graphics canvas(&bitmap);
        canvas.Clear(Color(255, 245, 247, 250));
        SolidBrush dark(Color(255, 25, 30, 38));
        canvas.FillRectangle(&dark, 0, 500, 740, 500);
        HDC dc = canvas.GetHDC();
        SetBkMode(dc, TRANSPARENT);
        for (int theme = 0; theme < 2; ++theme)
        {
            SetTextColor(dc, theme ? RGB(220, 225, 235) : RGB(35, 45, 60));
            const wchar_t* labels[] = {L"Off", L"Awake", L"Timer", L"Error", L"Display on", L"Error+display"};
            for (int state = 0; state < 6; ++state)
                TextOutW(dc, 160 + state * 92, theme * 500 + 12, labels[state], lstrlenW(labels[state]));
            for (int row = 0; row < 6; ++row)
            {
                const int dpis[] = {96, 96, 144, 144, 192, 192};
                const int heights[] = {18, 34, 27, 51, 36, 68};
                const wchar_t* names[] = {L"1 row / 100%", L"2 rows / 100%", L"1 row / 150%", L"2 rows / 150%", L"1 row / 200%", L"2 rows / 200%"};
                const int width = MulDiv(AwakePlugin::Instance().GetItem(0)->GetItemWidth(), dpis[row], 96);
                const int y = theme * 500 + 38 + row * 76;
                TextOutW(dc, 12, y + 8, names[row], lstrlenW(names[row]));
                for (int state = 0; state < 6; ++state)
                {
                    AwakeManager::Snapshot snap;
                    snap.mode = state == 0 ? AwakeManager::Mode::Off :
                        state == 2 ? AwakeManager::Mode::Timed : AwakeManager::Mode::Indefinite;
                    snap.requestApplied = state != 3 && state != 5;
                    snap.keepDisplayOn = state >= 4;
                    DrawStatusIcon(dc, 164 + state * 92, y, width, heights[row], snap, theme != 0, dpis[row]);
                }
            }
        }
        canvas.ReleaseHDC(dc);
        const CLSID pngEncoder = {0x557cf406, 0x1a04, 0x11d3, {0x9a, 0x73, 0x00, 0x00, 0xf8, 0x1e, 0xf3, 0x2e}};
        if (bitmap.Save(argv[1], &pngEncoder, nullptr) != Ok)
            result = 3;
    }
    GdiplusShutdown(token);
    return result;
}
